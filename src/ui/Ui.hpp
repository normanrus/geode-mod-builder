// Small UI helpers shared across the editor.
#pragma once

#include <Geode/Geode.hpp>
#include <functional>
#include <string>

namespace gmb {

// A dark themed, compact button that works with mouse and touch.
//
// Built on `CCMenuItemSpriteExtra` + `ButtonSprite`, the same primitives
// Geode's own UI uses, and created through `CCMenuItemExt::createSpriteExtra`
// so it accepts a lambda and works inside any `CCMenu`. `scale` is fixed so
// the label text stays readable at the requested size.
CCMenuItemSpriteExtra* makeButton(const std::string& text,
                                 std::function<void()> onPress,
                                 float scale = 0.5f);

// A dark label.
cocos2d::CCLabelBMFont* makeLabel(const std::string& text, const char* font = "bigFont.fnt", float scale = 0.5f);

// A rectangular dark panel using a GD nine-slice texture (a file name loaded by
// the engine), sized to `size`.
cocos2d::CCNodeRGBA* makePanel(cocos2d::CCSize const& size, cocos2d::ccColor3B color = {30, 30, 36}, GLubyte opacity = 200);

// Formats a float without trailing noise, for property fields.
std::string formatFloat(float v);

// Shows a transient notification (English UI text).
void notify(const std::string& text, geode::NotificationIcon icon = geode::NotificationIcon::Info);

} // namespace gmb
