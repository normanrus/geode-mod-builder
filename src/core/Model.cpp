#include "core/Model.hpp"

#include <algorithm>

namespace gmb {

// ---------------------------------------------------------------------------
// Element types
// ---------------------------------------------------------------------------

const char* elementTypeName(ElementType t) {
    switch (t) {
        case ElementType::Screen: return "Screen";
        case ElementType::Panel: return "Panel";
        case ElementType::Button: return "Button";
        case ElementType::Label: return "Label";
        case ElementType::Sprite: return "Sprite";
        case ElementType::Popup: return "Popup";
        case ElementType::Checkbox: return "Checkbox";
        case ElementType::Slider: return "Slider";
        case ElementType::TextInput: return "TextInput";
        case ElementType::CloseButton: return "CloseButton";
    }
    return "Unknown";
}

bool elementTypeFromName(const std::string& name, ElementType& out) {
    static const std::vector<ElementType> all = allElementTypes();
    for (auto t : all) {
        if (name == elementTypeName(t)) { out = t; return true; }
    }
    return false;
}

const std::vector<ElementType>& allElementTypes() {
    static const std::vector<ElementType> all = {
        ElementType::Screen, ElementType::Panel, ElementType::Button,
        ElementType::Label, ElementType::Sprite, ElementType::Popup,
        ElementType::Checkbox, ElementType::Slider, ElementType::TextInput,
        ElementType::CloseButton,
    };
    return all;
}

const std::vector<std::string>& anchorNames() {
    // Central list; index order also defines the anchor point mapping in the
    // generator (see anchorPointFor).
    static const std::vector<std::string> names = {
        "center", "topleft", "topright", "bottomleft", "bottomright",
        "left", "right", "top", "bottom",
    };
    return names;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

const char* actionTypeName(ActionType t) {
    switch (t) {
        case ActionType::None: return "None";
        case ActionType::OpenPopup: return "OpenPopup";
        case ActionType::ClosePopup: return "ClosePopup";
        case ActionType::OpenScreen: return "OpenScreen";
        case ActionType::ChangeLabelText: return "ChangeLabelText";
        case ActionType::ShowElement: return "ShowElement";
        case ActionType::HideElement: return "HideElement";
        case ActionType::ToggleVisibility: return "ToggleVisibility";
        case ActionType::SetEnabled: return "SetEnabled";
        case ActionType::SetOpacity: return "SetOpacity";
        case ActionType::SetValue: return "SetValue";
        case ActionType::SaveSetting: return "SaveSetting";
        case ActionType::Notify: return "Notify";
        case ActionType::RunAction: return "RunAction";
    }
    return "None";
}

bool actionTypeFromName(const std::string& name, ActionType& out) {
    for (auto t : allActionTypes()) {
        if (name == actionTypeName(t)) { out = t; return true; }
    }
    return false;
}

const std::vector<ActionType>& allActionTypes() {
    static const std::vector<ActionType> all = {
        ActionType::None, ActionType::OpenPopup, ActionType::ClosePopup,
        ActionType::OpenScreen, ActionType::ChangeLabelText,
        ActionType::ShowElement, ActionType::HideElement,
        ActionType::ToggleVisibility, ActionType::SetEnabled,
        ActionType::SetOpacity, ActionType::SetValue,
        ActionType::SaveSetting, ActionType::Notify, ActionType::RunAction,
    };
    return all;
}

const char* eventTypeName(EventType t) {
    switch (t) {
        case EventType::OnClick: return "OnClick";
        case EventType::OnPopupOpen: return "OnPopupOpen";
        case EventType::OnPopupClose: return "OnPopupClose";
        case EventType::OnScreenOpen: return "OnScreenOpen";
        case EventType::OnScreenClose: return "OnScreenClose";
        case EventType::OnValueChanged: return "OnValueChanged";
    }
    return "OnClick";
}

bool eventTypeFromName(const std::string& name, EventType& out) {
    for (auto t : allEventTypes()) {
        if (name == eventTypeName(t)) { out = t; return true; }
    }
    return false;
}

const std::vector<EventType>& allEventTypes() {
    static const std::vector<EventType> all = {
        EventType::OnClick, EventType::OnPopupOpen, EventType::OnPopupClose,
        EventType::OnScreenOpen, EventType::OnScreenClose, EventType::OnValueChanged,
    };
    return all;
}

// ---------------------------------------------------------------------------
// Serialization helpers
// ---------------------------------------------------------------------------

Json Element::toJson() const {
    Json j = Json::object();
    j["id"] = id;
    j["type"] = elementTypeName(type);
    j["name"] = name;
    if (!parent.empty()) j["parent"] = parent;
    if (!children.empty()) {
        Json arr = Json::array();
        for (const auto& c : children) arr.push(c);
        j["children"] = arr;
    }
    j["x"] = static_cast<double>(x);
    j["y"] = static_cast<double>(y);
    if (width != 0.f) j["width"] = static_cast<double>(width);
    if (height != 0.f) j["height"] = static_cast<double>(height);
    if (scale != 1.f) j["scale"] = static_cast<double>(scale);
    if (rotation != 0.f) j["rotation"] = static_cast<double>(rotation);
    if (opacity != 1.f) j["opacity"] = static_cast<double>(opacity);
    if (!visible) j["visible"] = visible;
    if (!enabled) j["enabled"] = enabled;
    if (zOrder != 0) j["zOrder"] = zOrder;
    if (anchor != "center") j["anchor"] = anchor;
    if (props.isObject() && !props.keys().empty()) j["props"] = props;
    return j;
}

Element Element::fromJson(const Json& j) {
    Element e;
    e.id = j["id"].asString();
    ElementType t;
    if (elementTypeFromName(j["type"].asString("Button"), t)) e.type = t;
    e.name = j["name"].asString();
    e.parent = j["parent"].asString();
    if (j["children"].isArray()) {
        for (const auto& c : j["children"].items()) e.children.push_back(c.asString());
    }
    e.x = static_cast<float>(j["x"].asNumber());
    e.y = static_cast<float>(j["y"].asNumber());
    e.width = static_cast<float>(j["width"].asNumber());
    e.height = static_cast<float>(j["height"].asNumber());
    e.scale = static_cast<float>(j["scale"].asNumber(1.0));
    e.rotation = static_cast<float>(j["rotation"].asNumber());
    e.opacity = static_cast<float>(j["opacity"].asNumber(1.0));
    e.visible = j["visible"].asBool(true);
    e.enabled = j["enabled"].asBool(true);
    e.zOrder = static_cast<int>(j["zOrder"].asInt());
    e.anchor = j["anchor"].asString("center");
    if (j["props"].isObject()) e.props = j["props"];
    return e;
}

Json Action::toJson() const {
    Json j = Json::object();
    j["id"] = id;
    j["type"] = actionTypeName(type);
    j["event"] = eventTypeName(event);
    if (params.isObject() && !params.keys().empty()) j["params"] = params;
    return j;
}

Action Action::fromJson(const Json& j) {
    Action a;
    a.id = j["id"].asString();
    actionTypeFromName(j["type"].asString("None"), a.type);
    eventTypeFromName(j["event"].asString("OnClick"), a.event);
    if (j["params"].isObject()) a.params = j["params"];
    return a;
}

Json Asset::toJson() const {
    Json j = Json::object();
    j["id"] = id;
    j["name"] = name;
    j["sourcePath"] = sourcePath;
    j["relativePath"] = relativePath;
    j["kind"] = kind;
    return j;
}

Asset Asset::fromJson(const Json& j) {
    Asset a;
    a.id = j["id"].asString();
    a.name = j["name"].asString();
    a.sourcePath = j["sourcePath"].asString();
    a.relativePath = j["relativePath"].asString();
    a.kind = j["kind"].asString("image");
    return a;
}

Json Screen::toJson() const {
    Json j = Json::object();
    j["id"] = id;
    j["name"] = name;
    Json arr = Json::array();
    for (const auto& r : roots) arr.push(r);
    j["roots"] = arr;
    return j;
}

Screen Screen::fromJson(const Json& j) {
    Screen s;
    s.id = j["id"].asString();
    s.name = j["name"].asString();
    if (j["roots"].isArray()) {
        for (const auto& r : j["roots"].items()) s.roots.push_back(r.asString());
    }
    return s;
}

Json ProjectMeta::toJson() const {
    Json j = Json::object();
    j["name"] = name;
    j["modId"] = modId;
    j["developer"] = developer;
    j["version"] = version;
    j["geodeVersion"] = geodeVersion;
    Json gd = Json::object();
    if (platformsWin) gd["win"] = gdWin;
    if (platformsAndroid) gd["android"] = gdAndroid;
    if (platformsMac) gd["mac"] = gdMac;
    if (platformsIos) gd["ios"] = gdIos;
    j["gd"] = gd;
    j["description"] = description;
    if (!icon.empty()) j["icon"] = icon;
    if (!createdAt.empty()) j["createdAt"] = createdAt;
    if (!modifiedAt.empty()) j["modifiedAt"] = modifiedAt;
    return j;
}

ProjectMeta ProjectMeta::fromJson(const Json& j) {
    ProjectMeta m;
    m.name = j["name"].asString();
    m.modId = j["modId"].asString();
    m.developer = j["developer"].asString();
    m.version = j["version"].asString("v1.0.0");
    m.geodeVersion = j["geodeVersion"].asString("v5.11.1");
    if (j["gd"].isObject()) {
        const auto& gd = j["gd"];
        if (gd.has("win")) { m.gdWin = gd["win"].asString("2.2081"); m.platformsWin = true; }
        if (gd.has("android")) { m.gdAndroid = gd["android"].asString("2.2081"); m.platformsAndroid = true; }
        if (gd.has("mac")) { m.gdMac = gd["mac"].asString("2.2081"); m.platformsMac = true; }
        if (gd.has("ios")) { m.gdIos = gd["ios"].asString("2.2081"); m.platformsIos = true; }
    }
    m.description = j["description"].asString();
    m.icon = j["icon"].asString();
    m.createdAt = j["createdAt"].asString();
    m.modifiedAt = j["modifiedAt"].asString();
    return m;
}

Json ProjectSetting::toJson() const {
    Json j = Json::object();
    j["id"] = id;
    j["key"] = key;
    j["type"] = type;
    j["value"] = value;
    return j;
}

ProjectSetting ProjectSetting::fromJson(const Json& j) {
    ProjectSetting s;
    s.id = j["id"].asString();
    s.key = j["key"].asString();
    s.type = j["type"].asString("bool");
    s.value = j["value"].asString();
    return s;
}

// ---------------------------------------------------------------------------
// Project
// ---------------------------------------------------------------------------

Element* Project::findElement(const std::string& id) {
    for (auto& e : elements) if (e.id == id) return &e;
    return nullptr;
}
const Element* Project::findElement(const std::string& id) const {
    for (const auto& e : elements) if (e.id == id) return &e;
    return nullptr;
}
Screen* Project::findScreen(const std::string& id) {
    for (auto& s : screens) if (s.id == id) return &s;
    return nullptr;
}
const Screen* Project::findScreen(const std::string& id) const {
    for (const auto& s : screens) if (s.id == id) return &s;
    return nullptr;
}
Asset* Project::findAsset(const std::string& id) {
    for (auto& a : assets) if (a.id == id) return &a;
    return nullptr;
}
const Asset* Project::findAsset(const std::string& id) const {
    for (const auto& a : assets) if (a.id == id) return &a;
    return nullptr;
}
Action* Project::findAction(const std::string& id) {
    for (auto& a : actions) if (a.id == id) return &a;
    return nullptr;
}
const Action* Project::findAction(const std::string& id) const {
    for (const auto& a : actions) if (a.id == id) return &a;
    return nullptr;
}
ProjectSetting* Project::findSetting(const std::string& id) {
    for (auto& s : settings) if (s.id == id) return &s;
    return nullptr;
}
const ProjectSetting* Project::findSetting(const std::string& id) const {
    for (const auto& s : settings) if (s.id == id) return &s;
    return nullptr;
}

std::string Project::newId(const std::string& prefix) {
    IdGenerator gen(nextId);
    std::string id = gen.make(prefix);
    nextId = gen.next();
    return id;
}

Json Project::toJson() const {
    Json j = Json::object();
    j["schema"] = schema;
    j["nextId"] = static_cast<int64_t>(nextId);
    j["meta"] = meta.toJson();
    Json scr = Json::array();
    for (const auto& s : screens) scr.push(s.toJson());
    j["screens"] = scr;
    Json els = Json::array();
    for (const auto& e : elements) els.push(e.toJson());
    j["elements"] = els;
    Json act = Json::array();
    for (const auto& a : actions) act.push(a.toJson());
    j["actions"] = act;
    Json ast = Json::array();
    for (const auto& a : assets) ast.push(a.toJson());
    j["assets"] = ast;
    Json st = Json::array();
    for (const auto& s : settings) st.push(s.toJson());
    j["settings"] = st;
    return j;
}

Project Project::fromJson(const Json& j) {
    Project p;
    p.schema = static_cast<int>(j["schema"].asInt(kSchemaVersion));
    p.nextId = static_cast<uint64_t>(j["nextId"].asInt(1));
    if (j["meta"].isObject()) p.meta = ProjectMeta::fromJson(j["meta"]);
    if (j["screens"].isArray()) for (const auto& s : j["screens"].items()) p.screens.push_back(Screen::fromJson(s));
    if (j["elements"].isArray()) for (const auto& e : j["elements"].items()) p.elements.push_back(Element::fromJson(e));
    if (j["actions"].isArray()) for (const auto& a : j["actions"].items()) p.actions.push_back(Action::fromJson(a));
    if (j["assets"].isArray()) for (const auto& a : j["assets"].items()) p.assets.push_back(Asset::fromJson(a));
    if (j["settings"].isArray()) for (const auto& s : j["settings"].items()) p.settings.push_back(ProjectSetting::fromJson(s));
    return p;
}

bool Project::deserialize(const std::string& text, Project& out, std::string& error) {
    Json j;
    if (!Json::parse(text, j, error)) return false;
    if (!j.isObject()) { error = "project root must be a JSON object"; return false; }
    out = fromJson(j);
    return true;
}

} // namespace gmb
