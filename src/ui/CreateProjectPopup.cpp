#include "ui/CreateProjectPopup.hpp"

#include "ui/Ui.hpp"

#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

namespace gmb {

bool CreateProjectPopup::init(std::function<void(Project)> onCreate) {
    if (!Popup::init(360.f, 260.f)) return false;
    m_onCreate = std::move(onCreate);
    this->setTitle("New Project");

    auto* menu = CCMenu::create();
    m_mainLayer->addChild(menu);

    float y = 120.f;
    auto addField = [&](const char* label, geode::TextInput*& field, const char* placeholder,
                        geode::CommonFilter filter, float width = 260.f) -> void {
        auto* lbl = makeLabel(label, "bigFont.fnt", 0.4f);
        lbl->setAnchorPoint({1.f, 0.5f});
        lbl->setPosition({-140.f, y});
        m_mainLayer->addChild(lbl);
        field = geode::TextInput::create(width, placeholder);
        field->setCommonFilter(filter);
        field->setPosition({0.f, y});
        m_mainLayer->addChild(field);
        y -= 28.f;
    };

    addField("Name", m_name, "My Mod", geode::CommonFilter::Any);
    addField("Mod ID", m_modId, "developer.mod-name", geode::CommonFilter::ID);
    addField("Developer", m_developer, "Your name", geode::CommonFilter::Any);
    addField("Version", m_version, "v1.0.0", geode::CommonFilter::Any, 120.f);
    m_version->setString("v1.0.0");
    addField("Geode", m_geode, "5.11.1", geode::CommonFilter::Any, 120.f);
    m_geode->setString("5.11.1");
    addField("Description", m_description, "Short description", geode::CommonFilter::Any);

    // Platform toggles.
    auto* platLabel = makeLabel("Platforms", "bigFont.fnt", 0.4f);
    platLabel->setAnchorPoint({1.f, 0.5f});
    platLabel->setPosition({-140.f, y});
    m_mainLayer->addChild(platLabel);

    auto mkToggle = [&](cocos2d::CCMenuItemToggler*& out, const char* text, float x) {
        out = CCMenuItemExt::createTogglerWithStandardSprites(0.7f, [](CCMenuItemToggler*) {});
        auto* lbl = makeLabel(text, "bigFont.fnt", 0.4f);
        lbl->setPosition({28.f, 0.f});
        out->addChild(lbl);
        out->setPosition({x, y});
        menu->addChild(out);
    };
    mkToggle(m_win, "Windows", -60.f);
    mkToggle(m_android, "Android", 40.f);
    mkToggle(m_mac, "macOS", 130.f);
    mkToggle(m_ios, "iOS", 210.f);

    // Defaults: Windows + Android on.
    if (m_win) m_win->toggle();
    if (m_android) m_android->toggle();

    auto* createBtn = makeButton("Create", [this]() { this->onCreateClicked(nullptr); }, 90.f);
    createBtn->setPosition({0.f, -110.f});
    menu->addChild(createBtn);

    return true;
}

void CreateProjectPopup::showErrors(const std::vector<Diagnostic>& diags) {
    std::string msg;
    for (const auto& d : diags) {
        if (d.severity != Severity::Error) continue;
        if (!msg.empty()) msg += "\n";
        msg += "- " + d.message;
    }
    if (msg.empty()) msg = "Unknown validation error.";
    FLAlertLayer::create("Cannot create project", msg.c_str(), "OK")->show();
}

void CreateProjectPopup::onCreateClicked(cocos2d::CCObject*) {
    Project p;
    p.meta.name = m_name->getString();
    p.meta.modId = m_modId->getString();
    p.meta.developer = m_developer->getString();
    p.meta.version = m_version->getString();
    p.meta.geodeVersion = m_geode->getString();
    p.meta.description = m_description->getString();
    p.meta.platformsWin = m_win && m_win->isToggled();
    p.meta.platformsAndroid = m_android && m_android->isToggled();
    p.meta.platformsMac = m_mac && m_mac->isToggled();
    p.meta.platformsIos = m_ios && m_ios->isToggled();

    auto vr = ProjectValidator::validateMeta(p.meta);
    if (vr.hasErrors()) { showErrors(vr.diagnostics); return; }

    // A fresh project always has one screen to start from.
    Screen scr;
    scr.id = p.newId("scr");
    scr.name = "Main";
    p.screens.push_back(scr);

    if (m_onCreate) m_onCreate(p);
    this->onClose(nullptr);
}

CreateProjectPopup* CreateProjectPopup::create(std::function<void(Project)> onCreate) {
    auto* ret = new CreateProjectPopup();
    if (ret->init(std::move(onCreate))) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

} // namespace gmb
