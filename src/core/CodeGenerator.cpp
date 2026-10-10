#include "core/CodeGenerator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace gmb {

// ---------------------------------------------------------------------------
// Low level helpers
// ---------------------------------------------------------------------------

std::string CodeGenerator::sanitizeIdentifier(const std::string& raw) {
    std::string out;
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') out.push_back(c);
        else out.push_back('_');
    }
    if (out.empty()) out = "_";
    if (std::isdigit(static_cast<unsigned char>(out[0]))) out = "_" + out;
    return out;
}

bool CodeGenerator::isValidCppIdentifier(const std::string& id) {
    if (id.empty()) return false;
    if (std::isdigit(static_cast<unsigned char>(id[0]))) return false;
    for (char c : id) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    static const std::set<std::string> keywords = {
        "class","struct","int","float","double","bool","char","void","return",
        "new","delete","this","namespace","template","typename","public",
        "private","protected","if","else","for","while","switch","case","default",
        "true","false","nullptr","const","static","auto","enum","union","using",
    };
    return keywords.count(id) == 0;
}

std::string CodeGenerator::cppStringLiteral(const std::string& raw) {
    std::string out = "\"";
    for (unsigned char c : raw) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\x%02x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out += '"';
    return out;
}

static std::string num(float v) {
    char buf[64];
    if (v == static_cast<float>(static_cast<long long>(v)))
        std::snprintf(buf, sizeof(buf), "%.1ff", static_cast<double>(v));
    else
        std::snprintf(buf, sizeof(buf), "%.4ff", static_cast<double>(v));
    return buf;
}

static std::string anchorPointFor(const std::string& anchor) {
    if (anchor == "bottomleft") return "ccp(0, 0)";
    if (anchor == "topleft") return "ccp(0, 1)";
    if (anchor == "topright") return "ccp(1, 1)";
    if (anchor == "bottomright") return "ccp(1, 0)";
    if (anchor == "left") return "ccp(0, 0.5f)";
    if (anchor == "right") return "ccp(1, 0.5f)";
    if (anchor == "top") return "ccp(0.5f, 1)";
    if (anchor == "bottom") return "ccp(0.5f, 0)";
    return "ccp(0.5f, 0.5f)";
}

static bool isContainer(ElementType t) {
    return t == ElementType::Panel || t == ElementType::Screen;
}

static std::string assetBasename(const std::string& rel) {
    auto pos = rel.find_last_of("/\\");
    return pos == std::string::npos ? rel : rel.substr(pos + 1);
}

// ---------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------

namespace {

struct GenContext {
    const Project* project = nullptr;
    std::unordered_map<std::string, std::vector<std::string>> children;
    // key: "<owner>|<event>"
    std::map<std::string, std::vector<const Action*>> actionsByOwnerEvent;
    std::vector<const Element*> ordered;
    std::map<std::string, const Element*> byId;
    bool hasPopup = false;
};

} // namespace

static GenContext buildContext(const Project& p) {
    GenContext ctx;
    ctx.project = &p;
    for (const auto& e : p.elements) ctx.byId[e.id] = &e;

    auto orderChildren = [&](std::vector<std::string> ids) {
        std::stable_sort(ids.begin(), ids.end(), [&](const std::string& a, const std::string& b) {
            int za = ctx.byId.count(a) ? ctx.byId[a]->zOrder : 0;
            int zb = ctx.byId.count(b) ? ctx.byId[b]->zOrder : 0;
            return za < zb;
        });
        return ids;
    };

    for (const auto& e : p.elements) {
        if (isContainer(e.type) || e.type == ElementType::Popup) {
            ctx.children[e.id] = orderChildren(e.children);
        }
    }

    std::set<std::string> visited;
    std::function<void(const std::string&)> walk = [&](const std::string& id) {
        if (!ctx.byId.count(id) || visited.count(id)) return;
        visited.insert(id);
        ctx.ordered.push_back(ctx.byId[id]);
        auto it = ctx.children.find(id);
        if (it != ctx.children.end())
            for (const auto& c : it->second) walk(c);
    };
    for (const auto& e : p.elements) if (e.parent.empty()) walk(e.id);
    for (const auto& e : p.elements) if (!visited.count(e.id)) walk(e.id);

    for (const auto& a : p.actions) {
        std::string owner = a.params["owner"].asString();
        ctx.actionsByOwnerEvent[owner + "|" + eventTypeName(a.event)].push_back(&a);
    }
    for (const auto& e : p.elements) if (e.type == ElementType::Popup) ctx.hasPopup = true;
    return ctx;
}

// ---------------------------------------------------------------------------
// Element creation
// ---------------------------------------------------------------------------

static void emitNodeSetup(std::ostringstream& o, const Element& e, const std::string& parentVar,
                          bool supportsOpacity) {
    o << "        node->setPosition(ccp(" << num(e.x) << ", " << num(e.y) << "));\n";
    o << "        node->setAnchorPoint(" << anchorPointFor(e.anchor) << ");\n";
    if (e.scale != 1.f) o << "        node->setScale(" << num(e.scale) << ");\n";
    if (e.rotation != 0.f) o << "        node->setRotation(" << num(e.rotation) << ");\n";
    // Only node types that are actually RGBA get an opacity write; CCMenu and
    // plain CCNode are not, and cross-binary RTTI casts are unreliable.
    if (supportsOpacity && e.opacity != 1.f)
        o << "        node->setOpacity(static_cast<GLubyte>(" << num(e.opacity) << " * 255.f));\n";
    if (!e.visible) o << "        node->setVisible(false);\n";
    o << "        " << parentVar << "->addChild(node, " << e.zOrder << ");\n";
    o << "        m_nodes[id] = node;\n";
    o << "        return node;\n";
}

static std::string generateCreateElement(const GenContext& ctx, const Element& e) {
    const Project& p = *ctx.project;
    std::ostringstream o;
    o << "    if (id == " << CodeGenerator::cppStringLiteral(e.id) << ") {\n";

    switch (e.type) {
        case ElementType::Panel:
        case ElementType::Screen: {
            o << "        auto* node = cocos2d::CCNode::create();\n";
            o << "        node->setContentSize(ccp(" << num(e.width) << ", " << num(e.height) << "));\n";
            o << "        node->setAnchorPoint(ccp(0, 0));\n";
            o << "        node->setPosition(ccp(" << num(e.x) << ", " << num(e.y) << "));\n";
            if (e.scale != 1.f) o << "        node->setScale(" << num(e.scale) << ");\n";
            if (!e.visible) o << "        node->setVisible(false);\n";
            o << "        parent->addChild(node, " << e.zOrder << ");\n";
            o << "        m_nodes[id] = node;\n";
            o << "        return node;\n";
            break;
        }
        case ElementType::Button: {
            std::string text = e.props["text"].asString(e.name.empty() ? e.id : e.name);
            std::string asset = e.props["asset"].asString();
            o << "        auto* node = geode::Button::create";
            if (!asset.empty()) {
                const Asset* a = p.findAsset(asset);
                std::string file = a ? assetBasename(a->relativePath) : assetBasename(asset);
                o << "WithSprite(" << CodeGenerator::cppStringLiteral(file) << ",\n";
            } else {
                o << "WithNode(ButtonSprite::create(" << CodeGenerator::cppStringLiteral(text) << "),\n";
            }
            o << "            [this](geode::Button*) { this->fire(" << CodeGenerator::cppStringLiteral(e.id)
              << ", \"OnClick\"); });\n";
            emitNodeSetup(o, e, "parent", true);
            break;
        }
        case ElementType::Label: {
            std::string text = e.props["text"].asString(e.name);
            std::string font = e.props["font"].asString("bigFont.fnt");
            o << "        auto* node = cocos2d::CCLabelBMFont::create(" << CodeGenerator::cppStringLiteral(text)
              << ", " << CodeGenerator::cppStringLiteral(font) << ");\n";
            float maxWidth = static_cast<float>(e.props["maxWidth"].asNumber(0.0));
            if (maxWidth > 0.f) o << "        node->setWidth(" << num(maxWidth) << ");\n";
            std::string color = e.props["color"].asString();
            if (color.size() == 7 && color[0] == '#') {
                int r = static_cast<int>(std::strtol(color.substr(1, 2).c_str(), nullptr, 16));
                int g = static_cast<int>(std::strtol(color.substr(3, 2).c_str(), nullptr, 16));
                int b = static_cast<int>(std::strtol(color.substr(5, 2).c_str(), nullptr, 16));
                o << "        node->setColor(ccc3(" << r << ", " << g << ", " << b << "));\n";
            }
            emitNodeSetup(o, e, "parent", true);
            break;
        }
        case ElementType::Sprite: {
            std::string asset = e.props["asset"].asString();
            const Asset* a = p.findAsset(asset);
            std::string file = a ? assetBasename(a->relativePath) : "GJ_button_01.png";
            if (a) {
                o << "        auto* node = cocos2d::CCSprite::create(" << CodeGenerator::cppStringLiteral(file) << ");\n";
            } else {
                o << "        auto* node = cocos2d::CCSprite::createWithSpriteFrameName("
                  << CodeGenerator::cppStringLiteral(file) << ");\n";
            }
            o << "        if (!node) node = cocos2d::CCSprite::createWithSpriteFrameName(\"GJ_button_01.png\");\n";
            if (e.width > 0.f)
                o << "        if (node->getContentSize().width > 0.f) node->setScaleX(" << num(e.width) << " / node->getContentSize().width);\n";
            if (e.height > 0.f)
                o << "        if (node->getContentSize().height > 0.f) node->setScaleY(" << num(e.height) << " / node->getContentSize().height);\n";
            emitNodeSetup(o, e, "parent", true);
            break;
        }
        case ElementType::Checkbox: {
            // CCMenuItemToggler must live in a CCMenu to receive touches.
            o << "        auto* node = cocos2d::CCMenu::create();\n";
            o << "        auto* toggler = CCMenuItemExt::createTogglerWithStandardSprites(1.f,\n";
            o << "            [this](CCMenuItemToggler*) { this->fire(" << CodeGenerator::cppStringLiteral(e.id)
              << ", \"OnValueChanged\"); });\n";
            o << "        if (toggler) { toggler->setPosition(ccp(0, 0)); node->addChild(toggler); }\n";
            o << "        node->setContentSize(ccp(30, 30));\n";
            emitNodeSetup(o, e, "parent", false);
            break;
        }
        case ElementType::Slider: {
            float mn = static_cast<float>(e.props["min"].asNumber(0.0));
            float mx = static_cast<float>(e.props["max"].asNumber(1.0));
            float val = static_cast<float>(e.props["value"].asNumber(0.0));
            float step = static_cast<float>(e.props["snapStep"].asNumber(0.0));
            o << "        auto* node = geode::SliderNode::create(\n";
            o << "            [this](geode::SliderNode*, float v) { this->m_values["
              << CodeGenerator::cppStringLiteral(e.id) << "] = v; this->fire("
              << CodeGenerator::cppStringLiteral(e.id) << ", \"OnValueChanged\"); });\n";
            o << "        node->setMin(" << num(mn) << ");\n";
            o << "        node->setMax(" << num(mx) << ");\n";
            o << "        node->setValue(" << num(val) << ");\n";
            if (step > 0.f) o << "        node->setSnapStep(" << num(step) << ");\n";
            emitNodeSetup(o, e, "parent", true);
            break;
        }
        case ElementType::TextInput: {
            float w = e.width > 0.f ? e.width : 160.f;
            std::string ph = e.props["placeholder"].asString("Type here...");
            std::string font = e.props["font"].asString("bigFont.fnt");
            o << "        auto* node = geode::TextInput::create(" << num(w) << ", "
              << CodeGenerator::cppStringLiteral(ph) << ", " << CodeGenerator::cppStringLiteral(font) << ");\n";
            int maxChars = static_cast<int>(e.props["maxChars"].asInt(0));
            if (maxChars > 0) o << "        node->setMaxCharCount(" << maxChars << ");\n";
            o << "        node->setCallback([this](std::string const&) { this->fire("
              << CodeGenerator::cppStringLiteral(e.id) << ", \"OnValueChanged\"); });\n";
            emitNodeSetup(o, e, "parent", false);
            break;
        }
        case ElementType::CloseButton: {
            o << "        auto* node = geode::Button::createWithSpriteFrameName(\"GJ_closeBtn_001.png\",\n";
            o << "            [this](geode::Button*) { this->fire(" << CodeGenerator::cppStringLiteral(e.id)
              << ", \"OnClick\"); });\n";
            emitNodeSetup(o, e, "parent", true);
            break;
        }
        case ElementType::Popup:
            o << "        return nullptr; // popups are created on demand\n";
            break;
    }
    o << "    }\n";
    return o.str();
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

static std::string generateActionBody(const GenContext& ctx, const Action& a) {
    const Project& p = *ctx.project;
    std::ostringstream o;
    o << "void GeneratedUI::doAction_" << CodeGenerator::sanitizeIdentifier(a.id) << "() {\n";
    std::string target = a.params["target"].asString();
    std::string tid = CodeGenerator::cppStringLiteral(target);

    switch (a.type) {
        case ActionType::OpenPopup: {
            o << "    if (auto* p = Popup_" << CodeGenerator::sanitizeIdentifier(target) << "::create(this)) {\n";
            o << "        p->setID(" << tid << ");\n";
            o << "        m_nodes[" << tid << "] = p;\n";
            o << "        p->show();\n";
            o << "        this->fire(" << tid << ", \"OnPopupOpen\");\n";
            o << "    }\n";
            break;
        }
        case ActionType::ClosePopup: {
            o << "    if (auto* n = this->node(" << tid << ")) {\n";
            o << "        if (auto* popup = typeinfo_cast<Popup_" << CodeGenerator::sanitizeIdentifier(target)
              << "*>(n)) popup->closeNow();\n";
            o << "    }\n";
            break;
        }
        case ActionType::OpenScreen:
            o << "    this->openScreen(" << tid << ");\n";
            break;
        case ActionType::ChangeLabelText: {
            std::string text = a.params["text"].asString();
            o << "    if (auto* n = this->node(" << tid << ")) {\n";
            o << "        if (auto* l = typeinfo_cast<cocos2d::CCLabelBMFont*>(n)) l->setString("
              << CodeGenerator::cppStringLiteral(text) << ");\n";
            o << "    }\n";
            break;
        }
        case ActionType::ShowElement:
        case ActionType::HideElement:
        case ActionType::ToggleVisibility: {
            o << "    if (auto* n = this->node(" << tid << ")) {\n";
            if (a.type == ActionType::ShowElement) o << "        n->setVisible(true);\n";
            else if (a.type == ActionType::HideElement) o << "        n->setVisible(false);\n";
            else o << "        n->setVisible(!n->isVisible());\n";
            o << "    }\n";
            break;
        }
        case ActionType::SetEnabled: {
            bool v = a.params["value"].asBool(true);
            o << "    if (auto* n = this->node(" << tid << ")) {\n";
            o << "        if (auto* b = typeinfo_cast<geode::Button*>(n)) b->setEnabled(" << (v ? "true" : "false") << ");\n";
            o << "    }\n";
            break;
        }
        case ActionType::SetOpacity: {
            double v = a.params["value"].asNumber(1.0);
            o << "    if (auto* n = this->node(" << tid << ")) {\n";
            o << "        if (auto* rgba = typeinfo_cast<cocos2d::CCNodeRGBA*>(n)) rgba->setOpacity(static_cast<GLubyte>("
              << num(static_cast<float>(v)) << " * 255.f));\n";
            o << "    }\n";
            break;
        }
        case ActionType::SetValue: {
            std::string sid = a.params["setting"].asString();
            const ProjectSetting* s = nullptr;
            for (const auto& ps : p.settings) if (ps.id == sid) s = &ps;
            std::string key = s ? s->key : sid;
            std::string val = a.params["value"].asString(s ? s->value : "");
            o << "    m_settings[" << CodeGenerator::cppStringLiteral(key) << "] = "
              << CodeGenerator::cppStringLiteral(val) << ";\n";
            break;
        }
        case ActionType::SaveSetting: {
            std::string sid = a.params["setting"].asString();
            const ProjectSetting* s = nullptr;
            for (const auto& ps : p.settings) if (ps.id == sid) s = &ps;
            std::string key = s ? s->key : sid;
            o << "    {\n";
            o << "        std::string v = m_settings.count(" << CodeGenerator::cppStringLiteral(key)
              << ") ? m_settings[" << CodeGenerator::cppStringLiteral(key) << "] : std::string("
              << CodeGenerator::cppStringLiteral(s ? s->value : "") << ");\n";
            o << "        Mod::get()->setSavedValue<std::string>(std::string(\"setting_\") + "
              << CodeGenerator::cppStringLiteral(key) << ", v);\n";
            o << "    }\n";
            break;
        }
        case ActionType::Notify: {
            std::string text = a.params["text"].asString();
            o << "    geode::Notification::create(" << CodeGenerator::cppStringLiteral(text)
              << ", geode::NotificationIcon::Info)->show();\n";
            break;
        }
        case ActionType::RunAction: {
            std::string t = a.params["action"].asString();
            o << "    this->doAction_" << CodeGenerator::sanitizeIdentifier(t) << "();\n";
            break;
        }
        case ActionType::None:
            o << "    // no-op\n";
            break;
    }
    o << "}\n\n";
    return o.str();
}

// ---------------------------------------------------------------------------
// Popups
// ---------------------------------------------------------------------------

static std::string generatePopupClass(const GenContext& /*ctx*/, const Element& popup) {
    std::ostringstream o;
    std::string cls = "Popup_" + CodeGenerator::sanitizeIdentifier(popup.id);
    std::string title = popup.props["title"].asString(popup.name);
    std::string message = popup.props["message"].asString();
    float w = popup.width > 0.f ? popup.width : 300.f;
    float h = popup.height > 0.f ? popup.height : 200.f;

    o << "class " << cls << " : public geode::Popup {\n";
    o << "protected:\n";
    o << "    GeneratedUI* m_owner = nullptr;\n";
    o << "    bool init(GeneratedUI* owner) {\n";
    o << "        if (!Popup::init(" << num(w) << ", " << num(h) << ")) return false;\n";
    o << "        m_owner = owner;\n";
    o << "        this->setTitle(" << CodeGenerator::cppStringLiteral(title) << ");\n";
    if (!message.empty()) {
        o << "        if (auto* lbl = cocos2d::CCLabelBMFont::create("
          << CodeGenerator::cppStringLiteral(message) << ", \"bigFont.fnt\")) {\n";
        o << "            lbl->setScale(0.55f);\n";
        o << "            lbl->setWidth(" << num(w - 40.f) << ");\n";
        o << "            m_mainLayer->addChildAtPosition(lbl, geode::Anchor::Top, ccp(0.f, -40.f));\n";
        o << "        }\n";
    }
    o << "        if (m_owner) m_owner->buildPopupChildren(" << CodeGenerator::cppStringLiteral(popup.id)
      << ", m_mainLayer);\n";
    o << "        return true;\n";
    o << "    }\n";
    o << "    void onClose(cocos2d::CCObject* sender) override {\n";
    o << "        if (m_owner) { m_owner->fire(" << CodeGenerator::cppStringLiteral(popup.id) << ", \"OnPopupClose\"); m_owner->forgetSubtree(" << CodeGenerator::cppStringLiteral(popup.id) << "); }\n";
    o << "        geode::Popup::onClose(sender);\n";
    o << "    }\n";
    o << "public:\n";
    o << "    static " << cls << "* create(GeneratedUI* owner) {\n";
    o << "        auto* ret = new " << cls << "();\n";
    o << "        if (ret->init(owner)) { ret->autorelease(); return ret; }\n";
    o << "        delete ret; return nullptr;\n";
    o << "    }\n";
    o << "    // Popup::onClose is protected, so expose a public close entry.\n";
    o << "    void closeNow() { this->onClose(nullptr); }\n";
    o << "};\n\n";
    return o.str();
}

// ---------------------------------------------------------------------------
// GeneratedUI header/source
// ---------------------------------------------------------------------------

static std::string generateGeneratedUIHeader(const GenContext& ctx) {
    std::ostringstream o;
    o << "#pragma once\n\n";
    o << "#include <Geode/Geode.hpp>\n";
    o << "#include <map>\n#include <string>\n#include <vector>\n\n";
    o << "class GeneratedUI : public cocos2d::CCLayer {\n";
    o << "public:\n";
    o << "    static GeneratedUI* create();\n";
    o << "    bool init() override;\n\n";
    o << "    cocos2d::CCNode* node(std::string const& id);\n";
    o << "    void openScreen(std::string const& id);\n";
    o << "    void fire(std::string const& owner, std::string const& event);\n";
    o << "    void forgetNode(std::string const& id);\n";
    o << "    void forgetSubtree(std::string const& id);\n";
    o << "    void buildPopupChildren(std::string const& popupId, cocos2d::CCNode* mainLayer);\n\n";
    for (const auto& a : ctx.project->actions)
        o << "    void doAction_" << CodeGenerator::sanitizeIdentifier(a.id) << "();\n";
    for (const auto& kv : ctx.actionsByOwnerEvent) {
        auto bar = kv.first.find('|');
        o << "    void fire_" << CodeGenerator::sanitizeIdentifier(kv.first.substr(0, bar)) << "_"
          << CodeGenerator::sanitizeIdentifier(kv.first.substr(bar + 1)) << "();\n";
    }
    o << "\nprotected:\n";
    o << "    cocos2d::CCNode* createElement(std::string const& id, cocos2d::CCNode* parent);\n";
    o << "    std::vector<std::string> childrenOf(std::string const& id);\n";
    o << "    std::map<std::string, cocos2d::CCNode*> m_nodes;\n";
    o << "    std::map<std::string, cocos2d::CCNode*> m_screens;\n";
    o << "    std::map<std::string, float> m_values;\n";
    o << "    std::map<std::string, std::string> m_settings;\n";
    o << "};\n";
    return o.str();
}

static std::string generateGeneratedUISource(const GenContext& ctx) {
    const Project& p = *ctx.project;
    std::ostringstream o;
    o << "// Generated by Geode Mod Builder. Do not edit by hand.\n";
    o << "#include \"GeneratedUI.hpp\"\n";
    o << "#include <Geode/Geode.hpp>\n";
    o << "#include <Geode/ui/Button.hpp>\n";
    o << "#include <Geode/ui/Popup.hpp>\n";
    o << "#include <Geode/ui/SliderNode.hpp>\n";
    o << "#include <Geode/ui/TextInput.hpp>\n";
    o << "#include <Geode/ui/Notification.hpp>\n";
    o << "using namespace geode::prelude;\n\n";

    for (const auto& e : p.elements)
        if (e.type == ElementType::Popup)
            o << "class Popup_" << CodeGenerator::sanitizeIdentifier(e.id) << ";\n";
    o << "\n";

    for (const auto& e : p.elements)
        if (e.type == ElementType::Popup) o << generatePopupClass(ctx, e);

    for (const auto& a : p.actions) o << generateActionBody(ctx, a);

    for (const auto& kv : ctx.actionsByOwnerEvent) {
        auto bar = kv.first.find('|');
        std::string owner = kv.first.substr(0, bar);
        std::string ev = kv.first.substr(bar + 1);
        o << "void GeneratedUI::fire_" << CodeGenerator::sanitizeIdentifier(owner) << "_"
          << CodeGenerator::sanitizeIdentifier(ev) << "() {\n";
        for (const auto* a : kv.second) o << "    this->doAction_" << CodeGenerator::sanitizeIdentifier(a->id) << "();\n";
        o << "}\n\n";
    }

    o << "cocos2d::CCNode* GeneratedUI::createElement(std::string const& id, cocos2d::CCNode* parent) {\n";
    for (const auto* e : ctx.ordered) o << generateCreateElement(ctx, *e);
    o << "    return nullptr;\n}\n\n";

    o << "std::vector<std::string> GeneratedUI::childrenOf(std::string const& id) {\n";
    for (const auto& kv : ctx.children) {
        o << "    if (id == " << CodeGenerator::cppStringLiteral(kv.first) << ") return {";
        for (size_t i = 0; i < kv.second.size(); i++) {
            if (i) o << ", ";
            o << CodeGenerator::cppStringLiteral(kv.second[i]);
        }
        o << "};\n";
    }
    o << "    return {};\n}\n\n";

    o << "cocos2d::CCNode* GeneratedUI::node(std::string const& id) {\n";
    o << "    auto it = m_nodes.find(id);\n    return it == m_nodes.end() ? nullptr : it->second;\n}\n\n";

    o << "void GeneratedUI::forgetNode(std::string const& id) {\n";
    o << "    m_nodes.erase(id);\n}\n\n";

    o << "void GeneratedUI::forgetSubtree(std::string const& id) {\n";
    o << "    for (auto const& child : this->childrenOf(id)) this->forgetSubtree(child);\n";
    o << "    m_nodes.erase(id);\n}\n\n";

    o << "void GeneratedUI::buildPopupChildren(std::string const& popupId, cocos2d::CCNode* mainLayer) {\n";
    o << "    for (auto const& child : this->childrenOf(popupId)) this->createElement(child, mainLayer);\n}\n\n";

    o << "void GeneratedUI::openScreen(std::string const& id) {\n";
    o << "    for (auto& kv : m_screens) if (kv.second) kv.second->setVisible(false);\n";
    o << "    auto it = m_screens.find(id);\n";
    o << "    if (it != m_screens.end() && it->second) it->second->setVisible(true);\n}\n\n";

    o << "void GeneratedUI::fire(std::string const& owner, std::string const& event) {\n";
    for (const auto& kv : ctx.actionsByOwnerEvent) {
        auto bar = kv.first.find('|');
        o << "    if (owner == " << CodeGenerator::cppStringLiteral(kv.first.substr(0, bar))
          << " && event == " << CodeGenerator::cppStringLiteral(kv.first.substr(bar + 1)) << ") { this->fire_"
          << CodeGenerator::sanitizeIdentifier(kv.first.substr(0, bar)) << "_"
          << CodeGenerator::sanitizeIdentifier(kv.first.substr(bar + 1)) << "(); return; }\n";
    }
    o << "}\n\n";

    o << "bool GeneratedUI::init() {\n";
    o << "    if (!CCLayer::init()) return false;\n";
    for (const auto& s : p.screens) {
        o << "    {\n";
        o << "        auto* scr = cocos2d::CCNode::create();\n";
        o << "        scr->setContentSize(ccp(480.f, 320.f));\n";
        o << "        this->addChild(scr);\n";
        o << "        m_screens[" << CodeGenerator::cppStringLiteral(s.id) << "] = scr;\n";
        for (const auto& root : s.roots)
            o << "        this->createElement(" << CodeGenerator::cppStringLiteral(root) << ", scr);\n";
        o << "    }\n";
    }
    if (p.screens.empty()) o << "    // project has no screens\n";
    for (const auto& st : p.settings) {
        o << "    m_settings[" << CodeGenerator::cppStringLiteral(st.key)
          << "] = Mod::get()->getSavedValue<std::string>(std::string(\"setting_\") + "
          << CodeGenerator::cppStringLiteral(st.key) << ", " << CodeGenerator::cppStringLiteral(st.value) << ");\n";
    }
    o << "    return true;\n}\n\n";

    o << "GeneratedUI* GeneratedUI::create() {\n";
    o << "    auto* ret = new GeneratedUI();\n";
    o << "    if (ret->init()) { ret->autorelease(); return ret; }\n";
    o << "    delete ret; return nullptr;\n}\n";
    return o.str();
}

// ---------------------------------------------------------------------------
// Metadata files
// ---------------------------------------------------------------------------

static std::string generateModJson(const Project& p) {
    Json m = Json::object();
    m["geode"] = p.meta.geodeVersion.empty() ? "5.11.1" : p.meta.geodeVersion;
    Json gd = Json::object();
    if (p.meta.platformsWin) gd["win"] = p.meta.gdWin;
    if (p.meta.platformsAndroid) gd["android"] = p.meta.gdAndroid;
    if (p.meta.platformsMac) gd["mac"] = p.meta.gdMac;
    if (p.meta.platformsIos) gd["ios"] = p.meta.gdIos;
    m["gd"] = gd;
    m["id"] = p.meta.modId;
    m["name"] = p.meta.name;
    m["version"] = p.meta.version;
    m["developer"] = p.meta.developer;
    m["description"] = p.meta.description;
    Json deps = Json::object();
    deps["geode.node-ids"] = ">=v1.23.3";
    m["dependencies"] = deps;

    std::vector<std::string> sprites;
    for (const auto& a : p.assets)
        if (a.kind == "image") sprites.push_back(a.relativePath);
    if (!sprites.empty()) {
        Json res = Json::object();
        Json arr = Json::array();
        for (const auto& s : sprites) arr.push(s);
        res["sprites"] = arr;
        m["resources"] = res;
    }
    return m.dump(4);
}

static std::string generateCmake(const Project& p) {
    std::string name = CodeGenerator::sanitizeIdentifier(
        p.meta.name.empty() ? "GeneratedMod" : p.meta.name);
    std::ostringstream o;
    o << "cmake_minimum_required(VERSION 3.21)\n";
    o << "set(CMAKE_CXX_STANDARD 23)\n";
    o << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
    o << "if (\"${CMAKE_SYSTEM_NAME}\" STREQUAL \"iOS\" OR IOS)\n";
    o << "    set(CMAKE_OSX_ARCHITECTURES \"arm64\")\n";
    o << "else()\n";
    o << "    set(CMAKE_OSX_ARCHITECTURES \"arm64;x86_64\")\n";
    o << "endif()\n";
    o << "set(CMAKE_CXX_VISIBILITY_PRESET hidden)\n\n";
    o << "project(" << name << " VERSION 1.0.0)\n\n";
    o << "file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)\n";
    o << "add_library(${PROJECT_NAME} SHARED ${SOURCES})\n\n";
    o << "if (NOT DEFINED ENV{GEODE_SDK})\n";
    o << "    message(FATAL_ERROR \"Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode\")\n";
    o << "else()\n";
    o << "    message(STATUS \"Found Geode: $ENV{GEODE_SDK}\")\n";
    o << "endif()\n\n";
    o << "add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)\n\n";
    o << "setup_geode_mod(${PROJECT_NAME})\n";
    return o.str();
}

static std::string generateMainCpp(const GenContext& ctx) {
    const Project& p = *ctx.project;
    std::string entry = p.meta.name.empty() ? "Mod UI" : p.meta.name;
    std::string id = p.meta.modId.empty() ? "generated.mod" : p.meta.modId;
    std::ostringstream o;
    o << "// Generated by Geode Mod Builder. Do not edit by hand.\n";
    o << "#include <Geode/Geode.hpp>\n";
    o << "#include <Geode/modify/MenuLayer.hpp>\n";
    o << "#include \"GeneratedUI.hpp\"\n";
    o << "using namespace geode::prelude;\n\n";
    o << "class $modify(GMBMenuLayer, MenuLayer) {\n";
    o << "    bool init() {\n";
    o << "        if (!MenuLayer::init()) return false;\n";
    o << "        auto* btn = CCMenuItemExt::createSpriteExtra(\n";
    o << "            ButtonSprite::create(" << CodeGenerator::cppStringLiteral(entry) << "),\n";
    o << "            [](CCMenuItemSpriteExtra*) {\n";
    o << "                auto* scene = CCScene::create();\n";
    o << "                scene->addChild(GeneratedUI::create());\n";
    o << "                CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5f, scene));\n";
    o << "            });\n";
    o << "        btn->setID(std::string(\"mod-builder-open-button-\" + "
      << CodeGenerator::cppStringLiteral(id) << ").c_str());\n";
    o << "        if (auto* menu = this->getChildByID(\"bottom-menu\")) {\n";
    o << "            menu->addChild(btn);\n";
    o << "            menu->updateLayout();\n";
    o << "        }\n";
    o << "        return true;\n";
    o << "    }\n";
    o << "};\n";
    return o.str();
}

static std::string generateAbout(const Project& p) {
    std::ostringstream o;
    o << "# " << (p.meta.name.empty() ? "Generated Mod" : p.meta.name) << "\n\n";
    if (!p.meta.description.empty()) o << p.meta.description << "\n\n";
    o << "Generated by Geode Mod Builder.\n";
    return o.str();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

GenerationResult CodeGenerator::generate(const Project& project) {
    GenerationResult result;

    ValidationResult vr = ProjectValidator::validate(project);
    result.diagnostics = vr.diagnostics;
    if (vr.hasErrors()) {
        result.ok = false;
        return result;
    }

    // Flattened resource names must be unique.
    {
        std::set<std::string> names;
        for (const auto& a : project.assets) {
            if (a.kind != "image") continue;
            std::string base = assetBasename(a.relativePath);
            if (!names.insert(base).second) {
                result.diagnostics.push_back({Severity::Error, "asset.name.duplicate",
                    "Two image assets share the file name '" + base +
                    "'. Rename one; generated resources flatten to file names.", a.id});
            }
        }
    }
    if (!result.diagnostics.empty()) {
        bool hasError = false;
        for (const auto& d : result.diagnostics) if (d.severity == Severity::Error) hasError = true;
        if (hasError) { result.ok = false; return result; }
    }

    // Two different ids that sanitize to the same C++ identifier would produce
    // duplicate symbols. Reject with a clear message instead of broken code.
    {
        std::set<std::string> seenIds, seenActions, seenPopups;
        for (const auto& e : project.elements) {
            std::string s = sanitizeIdentifier(e.id);
            if (!seenIds.insert(s).second)
                result.diagnostics.push_back({Severity::Error, "gen.ident.element",
                    "Element id '" + e.id + "' collides with another element in generated C++.", e.id});
            if (e.type == ElementType::Popup && !seenPopups.insert(s).second)
                result.diagnostics.push_back({Severity::Error, "gen.ident.popup",
                    "Popup id '" + e.id + "' collides with another popup in generated C++.", e.id});
        }
        for (const auto& a : project.actions) {
            if (!seenActions.insert(sanitizeIdentifier(a.id)).second)
                result.diagnostics.push_back({Severity::Error, "gen.ident.action",
                    "Action id '" + a.id + "' collides with another action in generated C++.", a.id});
        }
        bool hasError = false;
        for (const auto& d : result.diagnostics) if (d.severity == Severity::Error) hasError = true;
        if (hasError) { result.ok = false; return result; }
    }

    GenContext ctx = buildContext(project);

    auto addFile = [&](const std::string& path, const std::string& contents) {
        result.files.push_back({path, contents, false});
    };

    addFile("mod.json", generateModJson(project));
    addFile("CMakeLists.txt", generateCmake(project));
    addFile("src/main.cpp", generateMainCpp(ctx));
    addFile("src/GeneratedUI.hpp", generateGeneratedUIHeader(ctx));
    addFile("src/GeneratedUI.cpp", generateGeneratedUISource(ctx));
    addFile("about.md", generateAbout(project));
    addFile("resources/.gitkeep", "");

    // Copy assets as binary placeholders; ProjectManager replaces contents
    // with the real bytes from sourcePath at write time.
    for (const auto& a : project.assets) {
        if (a.kind == "other" || a.kind == "audio") continue;
        result.files.push_back({a.relativePath, "", true});
    }

    result.ok = true;
    return result;
}

} // namespace gmb
