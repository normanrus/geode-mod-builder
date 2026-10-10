// Action & event editor: manages a reliable list of actions attached to the
// selected element, with per-action parameters.
#pragma once

#include "ui/EditorState.hpp"

#include <Geode/ui/Popup.hpp>
#include <functional>

namespace gmb {

class ActionEditor : public geode::Popup {
protected:
    EditorState* m_state = nullptr;
    std::function<void(const std::string&)> m_commit;
    std::function<void()> m_onChanged;
    cocos2d::CCNode* m_list = nullptr;

    bool init(EditorState* state, std::function<void(const std::string&)> commit, std::function<void()> onChanged);
    void rebuild();
    void addAction(ActionType type, EventType event);
    void openParams(Action& a, size_t index);
    void showIfValid();

public:
    static ActionEditor* create(EditorState* state, std::function<void(const std::string&)> commit,
                                std::function<void()> onChanged);
};

} // namespace gmb
