// Build orchestration. The builder never runs arbitrary user commands: only a
// fixed `geode build` invocation, and only when the CLI is present and the user
// explicitly asks for it. When no toolchain is available it returns build
// instructions so the project can be built elsewhere.
#pragma once

#include "core/ProjectManager.hpp"

#include <string>
#include <vector>

namespace gmb {

struct BuildResult {
    bool attempted = false;   // whether a build command was actually run
    bool success = false;
    int exitCode = -1;
    std::string command;
    std::string output;
    std::string instructions; // always available
    std::vector<std::string> steps;
};

class BuildManager {
public:
    // Builds the human-readable instructions for a generated project.
    static std::vector<std::string> instructions(const std::string& outputRoot, const ProjectMeta& meta);

    // Attempts a build in `outputRoot`. If the CLI is missing or the user has
    // not confirmed, returns attempted=false with instructions populated.
    static BuildResult build(const std::string& outputRoot, const ProjectMeta& meta, bool userConfirmed);

    // Distinguishes generation errors from compilation errors for reporting.
    static std::string classifyOutput(const std::string& compilerOutput);
};

} // namespace gmb
