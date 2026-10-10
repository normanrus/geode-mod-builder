// Project creation dialog: collects metadata and validates it before creating.
#pragma once

#include "core/Model.hpp"
#include "core/ProjectValidator.hpp"

#include <Geode/ui/Popup.hpp>
#include <functional>
#include <string>

namespace gmb {

class CreateProjectPopup : public geode::Popup {
protected:
    geode::TextInput* m_name = nullptr;
    geode::TextInput* m_modId = nullptr;
    geode::TextInput* m_developer = nullptr;
    geode::TextInput* m_version = nullptr;
    geode::TextInput* m_geode = nullptr;
    geode::TextInput* m_description = nullptr;
    cocos2d::CCMenuItemToggler* m_win = nullptr;
    cocos2d::CCMenuItemToggler* m_android = nullptr;
    cocos2d::CCMenuItemToggler* m_mac = nullptr;
    cocos2d::CCMenuItemToggler* m_ios = nullptr;
    std::function<void(Project)> m_onCreate;

    bool init(std::function<void(Project)> onCreate);
    void onCreateClicked(cocos2d::CCObject*);
    void showErrors(const std::vector<Diagnostic>& diags);

public:
    static CreateProjectPopup* create(std::function<void(Project)> onCreate);
};

} // namespace gmb
