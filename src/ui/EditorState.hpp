// Shared editor state: the open project, its undo history and the file name.
// The model is kept separate from the temporary Cocos UI nodes; UI layers read
// and mutate the model, then rebuild their views.
#pragma once

#include "core/Model.hpp"
#include "core/ProjectManager.hpp"
#include "core/UndoRedoManager.hpp"

#include <memory>
#include <string>

namespace gmb {

struct EditorState {
    Project project;
    std::string fileName;          // e.g. "my.mod.json"
    std::string selectedId;        // selected element id ("" = none)
    bool dirty = false;            // unsaved changes present
    bool open = false;             // a project is currently loaded

    std::shared_ptr<ProjectManager> manager;
    std::unique_ptr<UndoRedoManager> undo = std::make_unique<UndoRedoManager>();

    // Commits an edit: pushes the current project onto the undo stack.
    void commit(const std::string& label) {
        undo->commit(project, label);
        dirty = true;
    }

    void resetHistory() {
        undo->reset(project);
        dirty = false;
    }
};

} // namespace gmb
