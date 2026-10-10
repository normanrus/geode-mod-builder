#include "core/ProjectManager.hpp"

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_set>

#ifdef _WIN32
#include <io.h>
#define GMB_POPEN _popen
#define GMB_PCLOSE _pclose
#else
#define GMB_POPEN popen
#define GMB_PCLOSE pclose
#endif

namespace fs = std::filesystem;

namespace gmb {

ProjectManager::ProjectManager(std::filesystem::path baseDir) : m_base(std::move(baseDir)) {}

void ProjectManager::ensureDirectories() const {
    std::error_code ec;
    fs::create_directories(projectsDir(), ec);
    fs::create_directories(generatedDir(), ec);
}

bool ProjectManager::isSafeName(const std::string& name) {
    if (name.empty() || name.size() > 128) return false;
    if (name == "." || name == "..") return false;
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|')
            return false;
        if (static_cast<unsigned char>(c) < 0x20) return false;
    }
    if (name.find("..") != std::string::npos) return false;
    return true;
}

std::string ProjectManager::sanitizeFileName(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.')
            out.push_back(c);
        else if (c == ' ')
            out.push_back('_');
    }
    if (out.empty() || out == "." || out == "..") out = "project";
    if (out.size() > 100) out.resize(100);
    return out;
}

std::vector<ProjectInfo> ProjectManager::listProjects() const {
    std::vector<ProjectInfo> out;
    std::error_code ec;
    if (!fs::exists(projectsDir(), ec)) return out;
    for (const auto& entry : fs::directory_iterator(projectsDir(), ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        ProjectInfo info;
        info.fileName = entry.path().filename().string();
        info.path = entry.path().string();

        std::ifstream in(entry.path());
        if (!in) continue;
        std::stringstream ss;
        ss << in.rdbuf();
        Project p;
        std::string err;
        if (Project::deserialize(ss.str(), p, err)) {
            info.displayName = p.meta.name.empty() ? info.fileName : p.meta.name;
            info.modifiedAt = p.meta.modifiedAt;
        } else {
            info.displayName = info.fileName + " (corrupted)";
        }
        out.push_back(info);
    }
    return out;
}

bool ProjectManager::saveProject(const Project& project, const std::string& fileName, std::string& error) const {
    if (!isSafeName(fileName)) { error = "Unsafe file name."; return false; }
    ensureDirectories();
    fs::path target = projectsDir() / fileName;

    std::string text = project.serialize();
    // Atomic-ish write: temp file in the same directory, then rename over.
    fs::path tmp = projectsDir() / (fileName + ".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) { error = "Cannot open temporary file for writing."; return false; }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) { error = "Failed to write temporary file."; return false; }
    }
    std::error_code ec;
    fs::rename(tmp, target, ec);
    if (ec) {
        // Fallback for platforms where rename over an existing file fails.
        fs::remove(target, ec);
        fs::rename(tmp, target, ec);
        if (ec) {
            fs::remove(tmp, ec);
            error = "Failed to replace project file: " + ec.message();
            return false;
        }
    }
    return true;
}

bool ProjectManager::saveProjectAs(const Project& project, const std::string& fileName, std::string& usedName, std::string& error) const {
    usedName = sanitizeFileName(fileName);
    if (usedName.find(".json") == std::string::npos) usedName += ".json";
    return saveProject(project, usedName, error);
}

bool ProjectManager::loadProject(const std::string& fileName, Project& out, std::string& error) const {
    if (!isSafeName(fileName)) { error = "Unsafe file name."; return false; }
    fs::path target = projectsDir() / fileName;
    std::ifstream in(target, std::ios::binary);
    if (!in) { error = "Project file not found."; return false; }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    if (!Project::deserialize(text, out, error)) {
        error = "Project file is corrupted or not valid JSON: " + error;
        return false;
    }
    if (out.schema > kSchemaVersion) {
        error = "Project schema is newer than this build (" + std::to_string(out.schema) +
                " > " + std::to_string(kSchemaVersion) + "). Update the mod.";
        return false;
    }
    // Forward migration hook (currently only schema 1 exists).
    return true;
}

bool ProjectManager::deleteProject(const std::string& fileName, std::string& error) const {
    if (!isSafeName(fileName)) { error = "Unsafe file name."; return false; }
    std::error_code ec;
    fs::path target = projectsDir() / fileName;
    if (!fs::exists(target, ec)) { error = "Project file not found."; return false; }
    fs::remove(target, ec);
    if (ec) { error = "Failed to delete project: " + ec.message(); return false; }
    return true;
}

WriteReport ProjectManager::writeGenerated(const Project& project, const GenerationResult& gen,
                                           const std::string& outputDirName) const {
    WriteReport r;
    if (!gen.ok) { r.message = "Generation failed; nothing was written."; return r; }

    std::string name = sanitizeFileName(outputDirName);
    if (name.empty()) name = sanitizeFileName(project.meta.name.empty() ? project.meta.modId : project.meta.name);

    ensureDirectories();
    fs::path root = generatedDir() / name;

    // Guard against writing outside the generated directory.
    fs::path canonicalBase = fs::weakly_canonical(generatedDir());
    fs::path canonicalRoot = fs::weakly_canonical(root);
    if (canonicalRoot.string().rfind(canonicalBase.string(), 0) != 0) {
        r.message = "Output path escapes the generated directory.";
        return r;
    }

    std::error_code ec;
    fs::create_directories(root, ec);

    std::ofstream log((root / "builder-report.txt"), std::ios::trunc);
    log << "Geode Mod Builder generation report\n\n";

    for (const auto& f : gen.files) {
        if (!isSafeName(f.relativePath) && f.relativePath.find('/') == std::string::npos &&
            f.relativePath.find('\\') == std::string::npos) {
            r.message = "Unsafe generated path: " + f.relativePath;
            return r;
        }
        if (f.relativePath.find("..") != std::string::npos) {
            r.message = "Generated path contains '..': " + f.relativePath;
            return r;
        }
        fs::path dest = root / f.relativePath;
        fs::create_directories(dest.parent_path(), ec);

        if (f.binary) {
            // Locate the asset for this path and copy its real bytes.
            const Asset* asset = nullptr;
            for (const auto& a : project.assets) if (a.relativePath == f.relativePath) { asset = &a; break; }
            if (!asset) { log << "MISSING ASSET RECORD " << f.relativePath << "\n"; continue; }
            std::string err;
            if (asset->sourcePath.empty() || !copyAsset(asset->sourcePath, dest, err)) {
                log << "MISSING SOURCE " << asset->sourcePath << " (" << err << ")\n";
                r.message = "Missing asset source for " + f.relativePath + ". Generation continued; copy it before building.";
            }
            continue;
        }

        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (!out) { r.message = "Cannot write generated file: " + f.relativePath; return r; }
        out.write(f.contents.data(), static_cast<std::streamsize>(f.contents.size()));
        r.writtenFiles.push_back(f.relativePath);
    }

    std::string verifyErr;
    bool structureOk = verifyGeneratedStructure(root, verifyErr);
    log << "\nStructure check: " << (structureOk ? "OK" : verifyErr) << "\n";
    log.close();

    r.outputRoot = root.string();
    r.ok = true;
    if (r.message.empty()) r.message = "Generation completed.";
    return r;
}

bool ProjectManager::verifyGeneratedStructure(const std::filesystem::path& root, std::string& error) {
    const char* required[] = {"mod.json", "CMakeLists.txt", "src/main.cpp", "src/GeneratedUI.cpp", "src/GeneratedUI.hpp"};
    for (const char* rel : required) {
        std::error_code ec;
        if (!fs::exists(root / rel, ec)) {
            error = std::string("Required file missing after generation: ") + rel;
            return false;
        }
    }
    return true;
}

bool ProjectManager::copyAsset(const std::filesystem::path& source, const std::filesystem::path& dest, std::string& error) {
    std::error_code ec;
    if (!fs::exists(source, ec)) { error = "source does not exist"; return false; }
    fs::create_directories(dest.parent_path(), ec);
    fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) { error = ec.message(); return false; }
    return true;
}

static bool runCapture(const std::string& cmd, std::string& output) {
    // Only used with fixed, non-user-controlled command strings.
    FILE* pipe = GMB_POPEN(cmd.c_str(), "r");
    if (!pipe) return false;
    char buf[256];
    output.clear();
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
    int rc = GMB_PCLOSE(pipe);
    return rc == 0;
}

ToolchainInfo ProjectManager::detectToolchain() {
    ToolchainInfo info;

    std::string out;
    if (runCapture("geode --version 2>&1", out)) {
        info.geodeCliFound = true;
        // First line usually contains the version.
        auto nl = out.find('\n');
        info.geodeCliVersion = out.substr(0, nl == std::string::npos ? out.size() : nl);
    }
#ifdef _WIN32
    info.geodeCliPath = info.geodeCliFound ? "geode.exe (on PATH)" : "";
#else
    {
        std::string which;
        if (runCapture("command -v geode 2>/dev/null", which)) {
            while (!which.empty() && (which.back() == '\n' || which.back() == '\r')) which.pop_back();
            info.geodeCliPath = which;
        }
    }
#endif

    if (const char* sdk = std::getenv("GEODE_SDK")) {
        info.sdkPath = sdk;
        std::error_code ec;
        info.sdkFound = fs::exists(sdk, ec);
    }

    if (runCapture("cmake --version 2>&1", out)) {
        info.cmakeFound = true;
        auto nl = out.find('\n');
        info.cmakePath = out.substr(0, nl == std::string::npos ? out.size() : nl);
    }

    std::string cc;
    if (runCapture("command -v clang++ 2>/dev/null || command -v g++ 2>/dev/null", cc)) {
        while (!cc.empty() && (cc.back() == '\n' || cc.back() == '\r')) cc.pop_back();
        if (!cc.empty()) { info.compilerFound = true; info.compilerPath = cc; }
    }

    if (!info.geodeCliFound) info.notes += "Geode CLI not found on PATH. Install it from the Geode CLI releases. ";
    if (!info.sdkFound) info.notes += "GEODE_SDK is not set. Configure the SDK before building. ";
    if (!info.cmakeFound) info.notes += "CMake not found. ";
    if (!info.compilerFound) info.notes += "No C++ compiler found. ";
    if (info.notes.empty()) info.notes = "Toolchain looks complete.";
    return info;
}

} // namespace gmb
