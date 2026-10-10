#include "ui/Ui.hpp"

#include <Geode/ui/Button.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>
#include <cstdio>

namespace gmb {

geode::Button* makeButton(const std::string& text, std::function<void()> onPress, float width, float scale) {
    auto* label = cocos2d::CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    if (label) label->setScale(scale);

    geode::Button* btn = nullptr;
    if (width > 0.f && label) {
        // Use a GD nine-slice frame name, not a file name.
        auto bg = geode::NineSlice::createWithSpriteFrameName("square02_001.png");
        if (bg) {
            bg->setContentSize({width, 22.f});
            bg->setColor({60, 60, 74});
            auto sz = bg->getContentSize();
            label->setPosition({sz.width / 2.f, sz.height / 2.f});
            bg->addChild(label);
            btn = geode::Button::createWithNode(bg, [onPress](geode::Button*) { if (onPress) onPress(); });
        }
    }
    if (!btn && label) {
        btn = geode::Button::createWithNode(label, [onPress](geode::Button*) { if (onPress) onPress(); });
    }
    return btn;
}

cocos2d::CCNode* makePanel(cocos2d::CCSize const& size, cocos2d::ccColor3B color, GLubyte opacity) {
    auto n = geode::NineSlice::createWithSpriteFrameName("square02_001.png");
    if (n) {
        n->setContentSize(size);
        n->setColor(color);
        n->setOpacity(opacity);
        n->setAnchorPoint({0, 0});
        n->setPosition({0, 0});
    }
    return n;
}

cocos2d::CCLabelBMFont* makeLabel(const std::string& text, const char* font, float scale) {
    auto* l = cocos2d::CCLabelBMFont::create(text.c_str(), font);
    if (l) l->setScale(scale);
    return l;
}

cocos2d::CCNode* makeDarkBackground(cocos2d::CCSize const& size, cocos2d::CCNode* parent) {
    auto* bg = makePanel(size, {20, 20, 24}, 240);
    if (bg && parent) parent->addChild(bg);
    return bg;
}

std::string formatFloat(float v) {
    char buf[32];
    if (v == static_cast<float>(static_cast<long long>(v)))
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    else
        std::snprintf(buf, sizeof(buf), "%.3f", static_cast<double>(v));
    return buf;
}

void notify(const std::string& text, geode::NotificationIcon icon) {
    geode::Notification::create(text, icon)->show();
}

} // namespace gmb
