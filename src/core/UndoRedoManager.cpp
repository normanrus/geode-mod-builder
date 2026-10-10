#include "core/UndoRedoManager.hpp"

namespace gmb {

void UndoRedoManager::reset(const Project& baseline) {
    m_states.clear();
    m_states.push_back({baseline.serialize(), "Initial"});
    m_index = 0;
}

void UndoRedoManager::commit(const Project& state, const std::string& label) {
    // Drop any redo tail.
    while (m_states.size() > m_index + 1) m_states.pop_back();
    m_states.push_back({state.serialize(), label});
    m_index = m_states.size() - 1;
    while (m_states.size() > m_limit) {
        m_states.pop_front();
        if (m_index > 0) m_index--;
    }
}

bool UndoRedoManager::apply(Project& out) const {
    std::string err;
    if (!Project::deserialize(m_states[m_index].json, out, err)) return false;
    out.schema = kSchemaVersion;
    return true;
}

bool UndoRedoManager::undo(Project& out) {
    if (!canUndo()) return false;
    m_index--;
    return apply(out);
}

bool UndoRedoManager::redo(Project& out) {
    if (!canRedo()) return false;
    m_index++;
    return apply(out);
}

std::string UndoRedoManager::undoLabel() const {
    if (!canUndo()) return "";
    return m_states[m_index].label;
}

std::string UndoRedoManager::redoLabel() const {
    if (!canRedo()) return "";
    return m_states[m_index + 1].label;
}

} // namespace gmb
