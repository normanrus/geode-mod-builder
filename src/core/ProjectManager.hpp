// Persistence: project list, save/load with atomic writes, generation output
// and asset copying, and toolchain detection.
//
// The manager never touches Geode or Cocos. It receives a base directory
// (the mod's save directory) from the caller, which keeps it testable and
// keeps all filesystem and path-safety logic in one place.
#pragma once

#include "core/CodeGenerator.hpp"
#include "core/Model.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace gmb {

struct ProjectInfo {
    std::string fileName;   // file name inside projectsDir
    std::string displayName;
    std::string modifiedAt;
    std::string path;
};

struct WriteReport {
    bool ok = false;
    std::string message;                       // English, user facing
    std::vector<std::string> writtenFiles;     // relative paths
    std::string outputRoot;                    // absolute path of generated project
};

struct ToolchainInfo {
    bool geodeCliFound = false;
    std::string geodeCliVersion;
    std::string geodeCliPath;
    bool sdkFound = false;
    std::string sdkPath;      // from GEODE_SDK env
    bool cmakeFound = false;
    std::string cmakePath;
    bool compilerFound = false;
    std::string compilerPath;
    std::string notes;
};

class ProjectManager {
public:
    explicit ProjectManager(std::filesystem::path baseDir);

    const std::filesystem::path& baseDir() const { return m_base; }
    std::filesystem::path projectsDir() const { return m_base / "projects"; }
    std::filesystem::path generatedDir() const { return m_base / "generated"; }

    void ensureDirectories() const;

    // --- project list -------------------------------------------------------
    std::vector<ProjectInfo> listProjects() const;

    // Validates a user supplied id/file name and prevents path traversal.
    static bool isSafeName(const std::string& name);
    static std::string sanitizeFileName(const std::string& name);

    // --- save / load --------------------------------------------------------
    // Atomic write through a temp file + rename. Never destructive without the
    // caller having prompted; the temp file is only renamed on success.
    bool saveProject(const Project& project, const std::string& fileName, std::string& error) const;

    // Save As under a new file name. Returns the file name actually used.
    bool saveProjectAs(const Project& project, const std::string& fileName, std::string& usedName, std::string& error) const;

    bool loadProject(const std::string& fileName, Project& out, std::string& error) const;

    bool deleteProject(const std::string& fileName, std::string& error) const;

    // --- generation ---------------------------------------------------------
    // Writes generated files into generated/<modId>/ (or an explicit outDir).
    // Asset files are copied from Asset::sourcePath without modifying the
    // originals. Missing source files are reported, not silently skipped.
    WriteReport writeGenerated(const Project& project, const GenerationResult& gen,
                               const std::string& outputDirName) const;

    // Checks that required files exist inside an output directory.
    static bool verifyGeneratedStructure(const std::filesystem::path& root, std::string& error);

    // --- toolchain ----------------------------------------------------------
    static ToolchainInfo detectToolchain();

    // Copies a single asset; returns the bytes-copy result and whether the
    // source exists.
    static bool copyAsset(const std::filesystem::path& source, const std::filesystem::path& dest, std::string& error);

private:
    std::filesystem::path m_base;
};

} // namespace gmb
