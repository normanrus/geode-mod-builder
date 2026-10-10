#include "core/BuildManager.hpp"

#include <array>
#include <cstdio>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define GMB_POPEN _popen
#define GMB_PCLOSE _pclose
#else
#include <sys/wait.h>
#define GMB_POPEN popen
#define GMB_PCLOSE pclose
#endif

namespace fs = std::filesystem;

namespace gmb {

std::vector<std::string> BuildManager::instructions(const std::string& outputRoot, const ProjectMeta& meta) {
    std::vector<std::string> steps;
    steps.push_back("Generated project: " + outputRoot);
    steps.push_back("1. Install the Geode CLI and SDK (see docs.geode-sdk.org).");
    steps.push_back("2. Set the GEODE_SDK environment variable to your SDK path.");
    steps.push_back("3. Open a terminal in the generated project folder.");
    steps.push_back("4. Run: geode build   (or: geode build --ninja)");
    steps.push_back("5. The built .geode package appears in the build folder.");
    if (!meta.modId.empty())
        steps.push_back("Mod ID: " + meta.modId);
    if (meta.platformsAndroid)
        steps.push_back("Android: build with geode build -p android64 after installing NDK binaries.");
    return steps;
}

std::string BuildManager::classifyOutput(const std::string& compilerOutput) {
    bool compileError = false;
    bool linkError = false;
    if (compilerOutput.find("error:") != std::string::npos) compileError = true;
    if (compilerOutput.find("undefined reference") != std::string::npos ||
        compilerOutput.find("ld returned") != std::string::npos) linkError = true;
    if (linkError) return "Link error while building the generated project.";
    if (compileError) return "Compilation error in the generated C++.";
    if (compilerOutput.find("Unable to find Geode SDK") != std::string::npos)
        return "Geode SDK not configured (GEODE_SDK).";
    return "Build finished.";
}

// Runs a fixed command line (only literal argv tokens, never user input) and
// captures combined stdout/stderr. Returns the exit code, or -1 on failure.
static int runFixed(const std::vector<std::string>& argv, const std::string& cwd, std::string& output) {
    output.clear();
    // Only fixed, literal tokens ever reach here (currently just "geode build").
    std::string cmd;
    for (size_t i = 0; i < argv.size(); i++) {
        if (i) cmd += ' ';
        cmd += argv[i];
    }
    if (!cwd.empty()) cmd = "cd \"" + cwd + "\" && " + cmd + " 2>&1";
    else cmd += " 2>&1";

    FILE* pipe = GMB_POPEN(cmd.c_str(), "r");
    if (!pipe) return -1;
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) output.append(buf, n);
    int rc = GMB_PCLOSE(pipe);
#ifdef _WIN32
    return rc;
#else
    if (rc == -1) return -1;
    return WEXITSTATUS(rc);
#endif
}

BuildResult BuildManager::build(const std::string& outputRoot, const ProjectMeta& meta, bool userConfirmed) {
    BuildResult r;
    r.instructions = "";
    auto steps = instructions(outputRoot, meta);
    r.steps = steps;
    for (const auto& s : steps) r.instructions += s + "\n";

    if (!userConfirmed) {
        r.attempted = false;
        return r;
    }

    ToolchainInfo tc = ProjectManager::detectToolchain();
    if (!tc.geodeCliFound) {
        r.attempted = false;
        r.success = false;
        r.output = "Geode CLI not found; build skipped. Use the instructions to build elsewhere.";
        return r;
    }

    // Verify the generated structure before attempting a build.
    std::string verifyErr;
    if (!ProjectManager::verifyGeneratedStructure(outputRoot, verifyErr)) {
        r.attempted = true;
        r.success = false;
        r.output = "Generated project is incomplete: " + verifyErr;
        return r;
    }

    r.attempted = true;
    r.command = "geode build";
    std::string out;
    int code = runFixed({"geode", "build"}, outputRoot, out);
    r.exitCode = code;
    r.output = out;
    r.success = (code == 0);
    return r;
}

} // namespace gmb
