// Command/snapshot history for the editor.
//
// Snapshot based: each committed edit stores a serialized Project. Snapshots
// cannot corrupt hierarchy or references because every restore is a full,
// validated deserialize. Memory is bounded by a configurable limit.
#pragma once

#include "core/Model.hpp"

#include <deque>
#include <functional>
#include <string>

namespace gmb {

class UndoRedoManager {
public:
    explicit UndoRedoManager(size_t limit = 100) : m_limit(limit) {}

    // Resets history to the given baseline (called after load / new project).
    void reset(const Project& baseline);

    // Records a new state. Returns the label to show in the UI.
    void commit(const Project& state, const std::string& label);

    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index + 1 < m_states.size(); }

    // Restores into `out`. Returns false when there is nothing to do.
    bool undo(Project& out);
    bool redo(Project& out);

    std::string undoLabel() const;
    std::string redoLabel() const;
    size_t depth() const { return m_states.size(); }

private:
    struct Entry {
        std::string json;
        std::string label;
    };
    std::deque<Entry> m_states;
    size_t m_index = 0;
    size_t m_limit;

    bool apply(Project& out) const;
};

} // namespace gmb
