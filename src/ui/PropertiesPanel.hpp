// Properties panel: edits the selected element. Only properties supported by
// the element's type are shown and applied.
#pragma once

#include "ui/EditorState.hpp"

#include <Geode/Geode.hpp>
#include <functional>

namespace gmb {

class PropertiesPanel : public cocos2d::CCNode {
protected:
    EditorState* m_state = nullptr;
    std::function<void(const std::string&)> m_commit;
    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCMenu* m_menu = nullptr;

    bool init(EditorState* state, std::function<void(const std::string&)> commit, cocos2d::CCSize size);
    void addRow(const std::string& label, cocos2d::CCNode* input, float& y);
    void onCommonText(const std::string& key, const std::string& value);
    void onCommonNumber(const std::string& key, const std::string& value);
    void onCommonBool(const std::string& key, bool value);
    void onPropText(const std::string& key, const std::string& value);
    void onPropNumber(const std::string& key, const std::string& value);
    void onDelete();

public:
    static PropertiesPanel* create(EditorState* state, std::function<void(const std::string&)> commit, cocos2d::CCSize size);
    void refresh();
};

} // namespace gmb
