#include "ui/Ui.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>
#include <cstdio>

using namespace geode::prelude;

namespace gmb {

CCMenuItemSpriteExtra* makeButton(const std::string& text, std::function<void()> onPress, float scale) {
    auto* spr = ButtonSprite::create(text.c_str());
    if (!spr) return nullptr;
    // Clamp to a sane range so text stays readable.
    if (scale < 0.3f) scale = 0.3f;
    if (scale > 1.0f) scale = 1.0f;
    spr->setScale(scale);
    return CCMenuItemExt::createSpriteExtra(
        spr, [onPress](CCMenuItemSpriteExtra*) { if (onPress) onPress(); });
}

cocos2d::CCNodeRGBA* makePanel(cocos2d::CCSize const& size, cocos2d::ccColor3B color, GLubyte opacity) {
    // Use the texture file name directly (not a sprite frame name). GD's
    // texture files always load, and NineSlice's default insets suit them.
    auto n = NineSlice::create("square02b_001.png");
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
