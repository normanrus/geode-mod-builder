// Project validation. Pure, UI-independent, used before generation and from
// the editor's status/validation panel.
#pragma once

#include "core/Model.hpp"

#include <string>
#include <vector>

namespace gmb {

enum class Severity { Error, Warning, Info };

struct Diagnostic {
    Severity severity = Severity::Error;
    std::string code;       // stable machine code, e.g. "meta.mod_id"
    std::string message;    // English, user facing (UI language is English)
    std::string elementId;  // optional subject
};

struct ValidationResult {
    std::vector<Diagnostic> diagnostics;
    bool hasErrors() const;
    bool ok() const { return !hasErrors(); }
    size_t errorCount() const;
    size_t warningCount() const;
};

// Validates metadata, ids, hierarchy, references, action params and assets.
class ProjectValidator {
public:
    static ValidationResult validate(const Project& project);

    // Metadata-only validation, used while editing the project settings form.
    static ValidationResult validateMeta(const ProjectMeta& meta);

    static bool isValidModId(const std::string& id);
    static bool isValidSemver(const std::string& version);
};

} // namespace gmb
