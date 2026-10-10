// Code generation: turns a validated Project into a real Geode C++ project.
//
// The generator is a pure module: it never touches Geode, the UI or the
// filesystem. It returns an in-memory file map, which ProjectManager writes to
// disk (using safe temp-file writes). This keeps generation testable without a
// toolchain and keeps path handling in one place.
#pragma once

#include "core/Model.hpp"
#include "core/ProjectValidator.hpp"

#include <string>
#include <vector>

namespace gmb {

struct GeneratedFile {
    std::string relativePath; // e.g. "src/main.cpp", "mod.json"
    std::string contents;
    bool binary = false;      // copied assets are binary
};

struct GenerationResult {
    bool ok = false;
    std::vector<GeneratedFile> files;
    std::vector<Diagnostic> diagnostics; // includes validator + generator errors
};

class CodeGenerator {
public:
    // Generates the full project. Rejects projects with validation errors and
    // returns the diagnostics with a clear reason instead of emitting broken
    // code.
    static GenerationResult generate(const Project& project);

    // Exposed for tests.
    static std::string sanitizeIdentifier(const std::string& raw);
    static std::string cppStringLiteral(const std::string& raw);
    static bool isValidCppIdentifier(const std::string& id);
};

} // namespace gmb
