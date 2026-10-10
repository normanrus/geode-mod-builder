// Project data model: metadata, screens, elements, actions, assets, settings.
//
// Pure data + serialization. No UI, no Geode. This is the single source of
// truth that the validator, code generator and editor all operate on.
#pragma once

#include "core/Ids.hpp"
#include "core/Json.hpp"

#include <string>
#include <vector>

namespace gmb {

// Bump when the on-disk shape changes; migrate() upgrades older files.
inline constexpr int kSchemaVersion = 1;

// ---------------------------------------------------------------------------
// Elements
// ---------------------------------------------------------------------------

// Only these element types are implemented end to end. Unknown types are
// rejected by the validator rather than silently emitted as broken code.
enum class ElementType {
    Screen,
    Panel,
    Button,
    Label,
    Sprite,
    Popup,
    Checkbox,
    Slider,
    TextInput,
    CloseButton,
};

const char* elementTypeName(ElementType t);
bool elementTypeFromName(const std::string& name, ElementType& out);
const std::vector<ElementType>& allElementTypes();

// Container types can hold children in the hierarchy.
inline bool isContainerType(ElementType t) {
    return t == ElementType::Panel || t == ElementType::Screen || t == ElementType::Popup;
}

// Anchor names and their corresponding cocos2d anchor points. Shared so the
// editor and the generator agree on the mapping.
const std::vector<std::string>& anchorNames();

// Common properties. Type-specific values live in `props`.
struct Element {
    std::string id;
    ElementType type = ElementType::Button;
    std::string name;
    std::string parent;               // empty => root of the screen
    std::vector<std::string> children; // ordered child element ids

    // Layout / visual common properties.
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
    float scale = 1.f;
    float rotation = 0.f;
    float opacity = 1.f;
    bool visible = true;
    bool enabled = true;
    int zOrder = 0;
    std::string anchor = "center";  // center | topleft | bottomleft | ...

    // Type-specific properties (text, texture, min/max, placeholder, ...).
    Json props = Json::object();

    Json toJson() const;
    static Element fromJson(const Json& j);
};

// ---------------------------------------------------------------------------
// Actions / events
// ---------------------------------------------------------------------------

enum class ActionType {
    None,
    OpenPopup,
    ClosePopup,
    OpenScreen,
    ChangeLabelText,
    ShowElement,
    HideElement,
    ToggleVisibility,
    SetEnabled,
    SetOpacity,
    SetValue,
    SaveSetting,
    Notify,
    RunAction,
};

const char* actionTypeName(ActionType t);
bool actionTypeFromName(const std::string& name, ActionType& out);
const std::vector<ActionType>& allActionTypes();

enum class EventType {
    OnClick,
    OnPopupOpen,
    OnPopupClose,
    OnScreenOpen,
    OnScreenClose,
    OnValueChanged,
};

const char* eventTypeName(EventType t);
bool eventTypeFromName(const std::string& name, EventType& out);
const std::vector<EventType>& allEventTypes();

// An action instance attached to an element. `params` carries the target ids
// and values the action needs; the validator checks they resolve.
struct Action {
    std::string id;
    ActionType type = ActionType::None;
    EventType event = EventType::OnClick;
    Json params = Json::object();

    Json toJson() const;
    static Action fromJson(const Json& j);
};

// ---------------------------------------------------------------------------
// Assets
// ---------------------------------------------------------------------------

struct Asset {
    std::string id;
    std::string name;
    std::string sourcePath;   // original path on disk (never modified)
    std::string relativePath; // path inside the generated project resources/
    std::string kind = "image"; // image | font | audio | other

    Json toJson() const;
    static Asset fromJson(const Json& j);
};

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

struct Screen {
    std::string id;
    std::string name;
    std::vector<std::string> roots; // top level element ids

    Json toJson() const;
    static Screen fromJson(const Json& j);
};

// ---------------------------------------------------------------------------
// Project
// ---------------------------------------------------------------------------

struct ProjectMeta {
    std::string name;
    std::string modId;      // geode mod id, lowercase, dots/dashes/underscores
    std::string developer;
    std::string version = "v1.0.0";
    std::string geodeVersion = "5.11.1";
    std::string gdWin = "2.2081";
    std::string gdAndroid = "2.2081";
    std::string gdMac = "2.2081";
    std::string gdIos = "2.2081";
    std::string description;
    std::string icon;       // asset id used as the mod logo, optional
    bool platformsWin = true;
    bool platformsAndroid = true;
    bool platformsMac = false;
    bool platformsIos = false;
    std::string createdAt;  // ISO-8601, informational
    std::string modifiedAt; // ISO-8601, informational

    Json toJson() const;
    static ProjectMeta fromJson(const Json& j);
};

// A named key/value the generated mod stores (SaveSetting action target).
struct ProjectSetting {
    std::string id;
    std::string key;
    std::string type = "bool"; // bool | int | float | string
    std::string value;

    Json toJson() const;
    static ProjectSetting fromJson(const Json& j);
};

struct Project {
    int schema = kSchemaVersion;
    ProjectMeta meta;
    std::vector<Screen> screens;
    std::vector<Element> elements;
    std::vector<Action> actions;
    std::vector<Asset> assets;
    std::vector<ProjectSetting> settings;
    uint64_t nextId = 1; // IdGenerator counter

    // Lookup helpers (return nullptr when not found).
    Element* findElement(const std::string& id);
    const Element* findElement(const std::string& id) const;
    Screen* findScreen(const std::string& id);
    const Screen* findScreen(const std::string& id) const;
    Asset* findAsset(const std::string& id);
    const Asset* findAsset(const std::string& id) const;
    Action* findAction(const std::string& id);
    const Action* findAction(const std::string& id) const;
    ProjectSetting* findSetting(const std::string& id);
    const ProjectSetting* findSetting(const std::string& id) const;

    std::string newId(const std::string& prefix);

    Json toJson() const;
    static Project fromJson(const Json& j);

    std::string serialize() const { return toJson().dump(2); }
    static bool deserialize(const std::string& text, Project& out, std::string& error);
};

} // namespace gmb
