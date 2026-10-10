// Small UI helpers shared across the editor.
#pragma once

#include <Geode/Geode.hpp>
#include <functional>
#include <string>

namespace gmb {

// A dark themed, compact button that works with mouse and touch.
geode::Button* makeButton(const std::string& text, std::function<void()> onPress, float width = 0.f, float scale = 0.55f);

// A dark label.
cocos2d::CCLabelBMFont* makeLabel(const std::string& text, const char* font = "bigFont.fnt", float scale = 0.5f);

// A rectangular dark panel using a GD nine-slice frame. Uses sprite frame names
// (valid in Geometry Dash) rather than file names.
cocos2d::CCNode* makePanel(cocos2d::CCSize const& size, cocos2d::ccColor3B color = {30, 30, 36}, GLubyte opacity = 200);

// A dark background node sized to the parent.
cocos2d::CCNode* makeDarkBackground(cocos2d::CCSize const& size, cocos2d::CCNode* parent);

// Formats a float without trailing noise, for property fields.
std::string formatFloat(float v);

// Shows a transient notification (English UI text).
void notify(const std::string& text, geode::NotificationIcon icon = geode::NotificationIcon::Info);

} // namespace gmb
