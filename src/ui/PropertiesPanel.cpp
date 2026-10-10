#include "ui/PropertiesPanel.hpp"

#include "ui/ActionEditor.hpp"
#include "ui/Ui.hpp"

#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>
#include <cstdlib>
#include <cstring>

using namespace geode::prelude;

namespace gmb {

bool PropertiesPanel::init(EditorState* state, std::function<void(const std::string&)> commit, cocos2d::CCSize size) {
    if (!CCNode::init()) return false;
    m_state = state;
    m_commit = std::move(commit);
    this->setContentSize(size);

    auto* scroll = ScrollLayer::create({size.width, size.height});
    scroll->setPosition({0.f, 0.f});
    this->addChild(scroll);
    m_content = scroll->m_contentLayer;

    // A CCMenu inside the scroll content so buttons and togglers receive
    // touches (a bare CCNode has no touch handling).
    m_menu = CCMenu::create();
    m_menu->setPosition({0.f, 0.f});
    m_content->addChild(m_menu);
    return true;
}

PropertiesPanel* PropertiesPanel::create(EditorState* state, std::function<void(const std::string&)> commit, cocos2d::CCSize size) {
    auto* ret = new PropertiesPanel();
    if (ret->init(state, std::move(commit), size)) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

void PropertiesPanel::addRow(const std::string& label, cocos2d::CCNode* input, float& y) {
    const float w = this->getContentSize().width;
    auto* lbl = makeLabel(label, "bigFont.fnt", 0.4f);
    lbl->setAnchorPoint({1.f, 0.5f});
    lbl->setPosition({w * 0.32f, y});
    m_content->addChild(lbl);
    if (input) {
        input->setPosition({w * 0.65f, y});
        // CCMenu only supports CCMenuItem children, so only menu items go into
        // the panel's menu; text inputs and labels handle their own display.
        bool isMenuItem = typeinfo_cast<cocos2d::CCMenuItem*>(input) != nullptr;
        if (isMenuItem && m_menu) m_menu->addChild(input);
        else m_content->addChild(input);
    }
    y -= 26.f;
}

static geode::TextInput* makeField(const std::string& value, geode::CommonFilter filter,
                                   std::function<void(const std::string&)> onChange, float width = 118.f) {
    auto* in = geode::TextInput::create(width, "");
    in->setCommonFilter(filter);
    in->setString(value);
    if (onChange) in->setCallback([onChange](std::string const& s) { onChange(s); });
    return in;
}

void PropertiesPanel::onCommonText(const std::string& key, const std::string& value) {
    auto* e = m_state->project.findElement(m_state->selectedId);
    if (!e) return;
    if (key == "name") e->name = value;
    else if (key == "anchor") e->anchor = value;
    if (m_commit) m_commit("Edit " + key);
}

void PropertiesPanel::onCommonNumber(const std::string& key, const std::string& value) {
    auto* e = m_state->project.findElement(m_state->selectedId);
    if (!e) return;
    if (value.empty()) return;
    float v = std::strtof(value.c_str(), nullptr);
    if (key == "x") e->x = v;
    else if (key == "y") e->y = v;
    else if (key == "width") e->width = v;
    else if (key == "height") e->height = v;
    else if (key == "scale") e->scale = v;
    else if (key == "rotation") e->rotation = v;
    else if (key == "opacity") e->opacity = std::max(0.f, std::min(1.f, v));
    else if (key == "zOrder") e->zOrder = static_cast<int>(v);
    if (m_commit) m_commit("Edit " + key);
}

void PropertiesPanel::onCommonBool(const std::string& key, bool value) {
    auto* e = m_state->project.findElement(m_state->selectedId);
    if (!e) return;
    if (key == "visible") e->visible = value;
    else if (key == "enabled") e->enabled = value;
    if (m_commit) m_commit("Edit " + key);
}

void PropertiesPanel::onPropText(const std::string& key, const std::string& value) {
    auto* e = m_state->project.findElement(m_state->selectedId);
    if (!e) return;
    e->props[key] = value;
    if (m_commit) m_commit("Edit " + key);
}

void PropertiesPanel::onPropNumber(const std::string& key, const std::string& value) {
    auto* e = m_state->project.findElement(m_state->selectedId);
    if (!e) return;
    if (value.empty()) return;
    e->props[key] = static_cast<double>(std::strtof(value.c_str(), nullptr));
    if (m_commit) m_commit("Edit " + key);
}

void PropertiesPanel::onDelete() {
    auto* e = m_state->project.findElement(m_state->selectedId);
    if (!e) return;
    createQuickPopup("Delete Element", "Delete the selected element?", "Cancel", "Delete",
        [this](FLAlertLayer*, bool btn2) {
            if (!btn2) return;
            // Reuse the same recursive delete logic as the tree by delegating:
            // simplest is to clear selection and remove the element here.
            Project& p = m_state->project;
            std::string id = m_state->selectedId;
            auto* el = p.findElement(id);
            if (!el) return;
            if (!el->parent.empty()) {
                if (auto* par = p.findElement(el->parent))
                    par->children.erase(std::remove(par->children.begin(), par->children.end(), id), par->children.end());
            } else {
                for (auto& s : p.screens)
                    s.roots.erase(std::remove(s.roots.begin(), s.roots.end(), id), s.roots.end());
            }
            p.elements.erase(std::remove_if(p.elements.begin(), p.elements.end(),
                [&](const Element& x) { return x.id == id; }), p.elements.end());
            p.actions.erase(std::remove_if(p.actions.begin(), p.actions.end(),
                [&](const Action& a) { return a.params["owner"].asString() == id; }), p.actions.end());
            m_state->selectedId.clear();
            if (m_commit) m_commit("Delete element");
        });
}

void PropertiesPanel::refresh() {
    if (!m_content) return;
    m_content->removeAllChildren();

    auto* e = m_state->project.findElement(m_state->selectedId);
    if (!e) {
        auto* none = makeLabel("No element selected.", "bigFont.fnt", 0.45f);
        none->setPosition({this->getContentSize().width / 2.f, 260.f});
        m_content->addChild(none);
        m_content->setContentSize({this->getContentSize().width, 300.f});
        return;
    }

    float y = 285.f;

    addRow("ID", makeLabel(e->id, "bigFont.fnt", 0.4f), y);
    addRow("Name", makeField(e->name, geode::CommonFilter::Any, [this](std::string const& s) { onCommonText("name", s); }), y);
    addRow("Position X", makeField(formatFloat(e->x), geode::CommonFilter::Float, [this](std::string const& s) { onCommonNumber("x", s); }, 90.f), y);
    addRow("Position Y", makeField(formatFloat(e->y), geode::CommonFilter::Float, [this](std::string const& s) { onCommonNumber("y", s); }, 90.f), y);

    bool sizeType = (e->type == ElementType::Panel || e->type == ElementType::Sprite ||
                     e->type == ElementType::Popup || e->type == ElementType::TextInput ||
                     e->type == ElementType::Slider);
    if (sizeType) {
        addRow("Width", makeField(formatFloat(e->width), geode::CommonFilter::Float, [this](std::string const& s) { onCommonNumber("width", s); }, 90.f), y);
        addRow("Height", makeField(formatFloat(e->height), geode::CommonFilter::Float, [this](std::string const& s) { onCommonNumber("height", s); }, 90.f), y);
    }

    addRow("Scale", makeField(formatFloat(e->scale), geode::CommonFilter::Float, [this](std::string const& s) { onCommonNumber("scale", s); }, 90.f), y);
    addRow("Rotation", makeField(formatFloat(e->rotation), geode::CommonFilter::Float, [this](std::string const& s) { onCommonNumber("rotation", s); }, 90.f), y);
    addRow("Opacity", makeField(formatFloat(e->opacity), geode::CommonFilter::Float, [this](std::string const& s) { onCommonNumber("opacity", s); }, 90.f), y);
    addRow("Z-Order", makeField(formatFloat(static_cast<float>(e->zOrder)), geode::CommonFilter::Int, [this](std::string const& s) { onCommonNumber("zOrder", s); }, 90.f), y);

    addRow("Anchor", makeButton(e->anchor, [this]() {
        static const std::vector<std::string> anchors = {"center","topleft","topright","bottomleft","bottomright","left","right","top","bottom"};
        auto* el = m_state->project.findElement(m_state->selectedId);
        if (!el) return;
        auto it = std::find(anchors.begin(), anchors.end(), el->anchor);
        size_t idx = it == anchors.end() ? 0 : (it - anchors.begin() + 1) % anchors.size();
        onCommonText("anchor", anchors[idx]);
        refresh();
    }, 0.4f), y);

    bool canToggleVis = true;
    if (canToggleVis) {
        const float tw = this->getContentSize().width;
        // Togglers live in the panel's menu so they receive touches.
        auto addToggle = [&](const char* label, bool value, std::function<void(bool)> apply) {
            addRow(label, nullptr, y);
            auto* tog = CCMenuItemExt::createTogglerWithStandardSprites(0.7f,
                [apply](CCMenuItemToggler* t) { apply(!t->isToggled()); });
            if (!value) tog->toggle(true);
            tog->setPosition({tw * 0.65f, y + 26.f});
            m_menu->addChild(tog);
        };
        addToggle("Visible", e->visible, [this](bool v) { onCommonBool("visible", v); });
        addToggle("Enabled", e->enabled, [this](bool v) { onCommonBool("enabled", v); });
    }

    // Parent info.
    std::string parentName = e->parent.empty() ? std::string("<root>") : e->parent;
    addRow("Parent", makeLabel(parentName, "bigFont.fnt", 0.4f), y);

    // --- Type specific -----------------------------------------------------
    if (e->type == ElementType::Label) {
        addRow("Text", makeField(e->props["text"].asString(), geode::CommonFilter::Any,
            [this](std::string const& s) { onPropText("text", s); }), y);
        addRow("Font", makeField(e->props["font"].asString("bigFont.fnt"), geode::CommonFilter::Any,
            [this](std::string const& s) { onPropText("font", s); }), y);
        addRow("Color #", makeField(e->props["color"].asString(), geode::CommonFilter::Any,
            [this](std::string const& s) { onPropText("color", s); }), y);
        addRow("Max Width", makeField(formatFloat(static_cast<float>(e->props["maxWidth"].asNumber(0.0))), geode::CommonFilter::Float,
            [this](std::string const& s) { onPropNumber("maxWidth", s); }, 90.f), y);
    } else if (e->type == ElementType::Button) {
        addRow("Text", makeField(e->props["text"].asString(), geode::CommonFilter::Any,
            [this](std::string const& s) { onPropText("text", s); }), y);
    } else if (e->type == ElementType::Sprite || e->type == ElementType::Button) {
        addRow("Texture", makeButton(e->props["asset"].asString().empty() ? std::string("(none)") : e->props["asset"].asString(),
            [this]() {
                auto* el = m_state->project.findElement(m_state->selectedId);
                if (!el) return;
                std::vector<std::string> ids;
                ids.push_back(""); // none
                for (auto& a : m_state->project.assets) if (a.kind == "image") ids.push_back(a.id);
                auto cur = el->props["asset"].asString();
                auto it = std::find(ids.begin(), ids.end(), cur);
                size_t idx = it == ids.end() ? 0 : (it - ids.begin() + 1) % ids.size();
                onPropText("asset", ids[idx]);
                refresh();
            }, 0.4f), y);
    } else if (e->type == ElementType::Popup) {
        addRow("Title", makeField(e->props["title"].asString(), geode::CommonFilter::Any,
            [this](std::string const& s) { onPropText("title", s); }), y);
        addRow("Message", makeField(e->props["message"].asString(), geode::CommonFilter::Any,
            [this](std::string const& s) { onPropText("message", s); }), y);
    } else if (e->type == ElementType::Slider) {
        addRow("Min", makeField(formatFloat(static_cast<float>(e->props["min"].asNumber(0.0))), geode::CommonFilter::Float,
            [this](std::string const& s) { onPropNumber("min", s); }, 90.f), y);
        addRow("Max", makeField(formatFloat(static_cast<float>(e->props["max"].asNumber(1.0))), geode::CommonFilter::Float,
            [this](std::string const& s) { onPropNumber("max", s); }, 90.f), y);
        addRow("Value", makeField(formatFloat(static_cast<float>(e->props["value"].asNumber(0.0))), geode::CommonFilter::Float,
            [this](std::string const& s) { onPropNumber("value", s); }, 90.f), y);
        addRow("Snap Step", makeField(formatFloat(static_cast<float>(e->props["snapStep"].asNumber(0.0))), geode::CommonFilter::Float,
            [this](std::string const& s) { onPropNumber("snapStep", s); }, 90.f), y);
    } else if (e->type == ElementType::TextInput) {
        addRow("Placeholder", makeField(e->props["placeholder"].asString(), geode::CommonFilter::Any,
            [this](std::string const& s) { onPropText("placeholder", s); }), y);
        addRow("Max Chars", makeField(formatFloat(static_cast<float>(e->props["maxChars"].asInt(0))), geode::CommonFilter::Int,
            [this](std::string const& s) { onPropNumber("maxChars", s); }, 90.f), y);
    }

    // Action / event count + editor button.
    int actionCount = 0;
    for (auto& a : m_state->project.actions) if (a.params["owner"].asString() == e->id) actionCount++;
    addRow("Actions", makeLabel(std::to_string(actionCount), "bigFont.fnt", 0.4f), y);

    const float pw = this->getContentSize().width;
    m_content->setContentSize({pw, std::max(this->getContentSize().height, 300.f + (285.f - y))});

    auto* deleteBtn = makeButton("Delete Element", [this]() { this->onDelete(); }, 0.45f);
    deleteBtn->setPosition({pw / 2.f, y - 6.f});
    m_menu->addChild(deleteBtn);
    y -= 40.f;

    auto* actionsBtn = makeButton("Edit Actions", [this]() {
        ActionEditor::create(m_state, m_commit, [this]() { if (m_commit) m_commit("Edit actions"); })->show();
    }, 0.45f);
    actionsBtn->setPosition({pw / 2.f, y - 6.f});
    m_menu->addChild(actionsBtn);
}

} // namespace gmb
