// Element hierarchy: selection, rename, duplicate, delete, reorder, nest into
// containers, jump to parent, and error badges for missing references.
#pragma once

#include "ui/EditorState.hpp"

#include <Geode/Geode.hpp>
#include <functional>
#include <string>

namespace gmb {

class ElementTree : public cocos2d::CCNode {
protected:
    EditorState* m_state = nullptr;
    std::function<void(const std::string&)> m_commit; // commit + rebuild
    std::shared_ptr<ValidationResult> m_validation;
    cocos2d::CCNode* m_list = nullptr;
    cocos2d::CCMenu* m_menu = nullptr;

    bool init(EditorState* state, std::function<void(const std::string&)> commit, cocos2d::CCSize size);

public:
    static ElementTree* create(EditorState* state, std::function<void(const std::string&)> commit, cocos2d::CCSize size);
    void setValidation(std::shared_ptr<ValidationResult> v) { m_validation = std::move(v); }
    void refresh();

    // Actions (public so toolbar buttons can call them too).
    void addElement(ElementType type);
    void renameSelected();
    void duplicateSelected();
    void deleteSelected();
    void moveSelected(int delta);
    void nestSelected();
    void selectParent();
};

} // namespace gmb
