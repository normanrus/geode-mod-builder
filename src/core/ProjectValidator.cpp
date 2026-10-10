#include "core/ProjectValidator.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace gmb {

bool ValidationResult::hasErrors() const {
    for (const auto& d : diagnostics) if (d.severity == Severity::Error) return true;
    return false;
}
size_t ValidationResult::errorCount() const {
    size_t n = 0;
    for (const auto& d : diagnostics) if (d.severity == Severity::Error) n++;
    return n;
}
size_t ValidationResult::warningCount() const {
    size_t n = 0;
    for (const auto& d : diagnostics) if (d.severity == Severity::Warning) n++;
    return n;
}

static void add(ValidationResult& r, Severity s, const std::string& code,
                const std::string& msg, const std::string& subject = "") {
    r.diagnostics.push_back({s, code, msg, subject});
}

bool ProjectValidator::isValidModId(const std::string& id) {
    if (id.empty()) return false;
    // Geode: lowercase ASCII a-z, '.', '_', '-'. Must not be only separators.
    bool hasAlnum = false;
    for (char c : id) {
        if (c >= 'a' && c <= 'z') { hasAlnum = true; continue; }
        if (c == '.' || c == '_' || c == '-') continue;
        return false;
    }
    return hasAlnum;
}

bool ProjectValidator::isValidSemver(const std::string& version) {
    // Accept "1.2.3" or "v1.2.3".
    std::string v = version;
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v = v.substr(1);
    if (v.empty()) return false;
    int dots = 0;
    bool digitInPart = false;
    for (size_t i = 0; i < v.size(); i++) {
        char c = v[i];
        if (c == '.') {
            if (!digitInPart) return false;
            dots++;
            digitInPart = false;
            if (dots > 2) return false;
        } else if (c >= '0' && c <= '9') {
            digitInPart = true;
        } else {
            // Allow pre-release/build metadata after a dash for the first part.
            return dots >= 2 && c == '-';
        }
    }
    return digitInPart && dots == 2;
}

ValidationResult ProjectValidator::validateMeta(const ProjectMeta& meta) {
    ValidationResult r;
    if (meta.name.empty()) add(r, Severity::Error, "meta.name", "Project name is required.");
    if (meta.modId.empty()) add(r, Severity::Error, "meta.mod_id", "Mod ID is required.");
    else if (!isValidModId(meta.modId))
        add(r, Severity::Error, "meta.mod_id",
            "Mod ID may only contain lowercase letters, digits, '.', '_' and '-' (and must start with a letter).");
    if (meta.developer.empty()) add(r, Severity::Error, "meta.developer", "Developer name is required.");
    if (meta.version.empty()) add(r, Severity::Error, "meta.version", "Version is required.");
    else if (!isValidSemver(meta.version))
        add(r, Severity::Error, "meta.version", "Version must be semver, e.g. v1.0.0.");
    if (meta.geodeVersion.empty()) add(r, Severity::Error, "meta.geode", "Target Geode version is required.");
    if (!meta.platformsWin && !meta.platformsAndroid && !meta.platformsMac && !meta.platformsIos)
        add(r, Severity::Error, "meta.platforms", "Select at least one target platform.");
    return r;
}

ValidationResult ProjectValidator::validate(const Project& p) {
    ValidationResult r = validateMeta(p.meta);

    // --- Unique ids across all entity kinds ---------------------------------
    std::unordered_set<std::string> ids;
    auto addId = [&](const std::string& id, const char* kind, const std::string& subj) {
        if (id.empty()) {
            add(r, Severity::Error, "id.empty", std::string("An element/action with an empty id exists (" ) + kind + ").", subj);
            return;
        }
        if (!ids.insert(id).second) {
            add(r, Severity::Error, "id.duplicate",
                std::string("Duplicate id '") + id + "' (" + kind + "). Ids must be globally unique.", subj);
        }
    };
    for (const auto& e : p.elements) addId(e.id, "element", e.id);
    for (const auto& a : p.actions) addId(a.id, "action", a.id);
    for (const auto& s : p.screens) addId(s.id, "screen", s.id);
    for (const auto& a : p.assets) addId(a.id, "asset", a.id);

    // --- Elements -----------------------------------------------------------
    std::unordered_map<std::string, const Element*> byId;
    for (const auto& e : p.elements) byId[e.id] = &e;

    for (const auto& e : p.elements) {
        if (e.type == ElementType::Label) {
            if (e.props["text"].isNull() && e.name.empty())
                add(r, Severity::Warning, "label.text", "Label has no text.", e.id);
        }
        if (e.type == ElementType::Sprite) {
            std::string assetId = e.props["asset"].asString();
            if (assetId.empty()) {
                add(r, Severity::Error, "sprite.asset", "Sprite has no texture assigned.", e.id);
            } else if (!p.findAsset(assetId)) {
                add(r, Severity::Error, "sprite.asset.missing",
                    "Sprite references a missing asset '" + assetId + "'.", e.id);
            }
        }
        if (e.type == ElementType::Slider) {
            double mn = e.props["min"].asNumber(0.0);
            double mx = e.props["max"].asNumber(1.0);
            if (mx <= mn) add(r, Severity::Error, "slider.range", "Slider max must be greater than min.", e.id);
        }
        if (e.type == ElementType::Checkbox || e.type == ElementType::Slider) {
            // These can host OnValueChanged actions.
        }
        // Parent reference must exist and must be a container.
        if (!e.parent.empty()) {
            auto it = byId.find(e.parent);
            if (it == byId.end()) {
                add(r, Severity::Error, "element.parent.missing",
                    "Parent '" + e.parent + "' does not exist.", e.id);
            } else {
                ElementType pt = it->second->type;
                if (pt != ElementType::Panel && pt != ElementType::Popup && pt != ElementType::Screen) {
                    add(r, Severity::Error, "element.parent.type",
                        "Parent must be a Panel, Popup or Screen.", e.id);
                }
                // Child must also list the parent (bidirectional consistency).
                const auto& pc = it->second->children;
                if (std::find(pc.begin(), pc.end(), e.id) == pc.end()) {
                    add(r, Severity::Error, "element.parent.inconsistent",
                        "Parent does not list this element as a child.", e.id);
                }
            }
        }
        // Declared children must exist and point back.
        for (const auto& cid : e.children) {
            auto it = byId.find(cid);
            if (it == byId.end()) {
                add(r, Severity::Error, "element.child.missing",
                    "Child '" + cid + "' does not exist.", e.id);
            } else if (it->second->parent != e.id) {
                add(r, Severity::Error, "element.child.inconsistent",
                    "Child '" + cid + "' does not declare this element as its parent.", e.id);
            }
        }
        // No element may be its own parent.
        if (e.parent == e.id)
            add(r, Severity::Error, "element.parent.self", "Element is its own parent.", e.id);
    }

    // Cycle detection over the parent graph.
    {
        enum class Mark { White, Gray, Black };
        std::unordered_map<std::string, Mark> mark;
        for (const auto& e : p.elements) mark[e.id] = Mark::White;
        std::function<bool(const std::string&)> visit = [&](const std::string& id) -> bool {
            mark[id] = Mark::Gray;
            const Element* e = byId[id];
            if (e && !e->parent.empty() && byId.count(e->parent)) {
                if (mark[e->parent] == Mark::Gray) return true; // back edge => cycle
                if (mark[e->parent] == Mark::White && visit(e->parent)) return true;
            }
            mark[id] = Mark::Black;
            return false;
        };
        for (const auto& e : p.elements) {
            if (mark[e.id] == Mark::White && visit(e.id)) {
                add(r, Severity::Error, "element.cycle",
                    "Element hierarchy contains a cycle.", e.id);
                break;
            }
        }
    }

    // Screens reference existing root elements that have no parent.
    for (const auto& s : p.screens) {
        if (s.name.empty()) add(r, Severity::Warning, "screen.name", "Screen has no name.", s.id);
        std::unordered_set<std::string> seen;
        for (const auto& rid : s.roots) {
            if (!seen.insert(rid).second)
                add(r, Severity::Warning, "screen.root.duplicate", "Screen lists a root element twice.", s.id);
            auto it = byId.find(rid);
            if (it == byId.end()) {
                add(r, Severity::Error, "screen.root.missing",
                    "Screen root '" + rid + "' does not exist.", s.id);
            } else if (it->second->parent.empty() == false) {
                add(r, Severity::Error, "screen.root.parented",
                    "Screen root '" + rid + "' has a parent; roots must be top level.", s.id);
            }
        }
    }

    // --- Actions ------------------------------------------------------------
    for (const auto& a : p.actions) {
        // Owner (the source element) is stored in params["owner"].
        std::string owner = a.params["owner"].asString();
        if (owner.empty()) {
            add(r, Severity::Error, "action.owner", "Action has no owner element.", a.id);
        } else if (!byId.count(owner)) {
            add(r, Severity::Error, "action.owner.missing",
                "Action owner '" + owner + "' does not exist.", a.id);
        }
        if (a.type == ActionType::None)
            add(r, Severity::Error, "action.type", "Action has no type.", a.id);

        switch (a.type) {
            case ActionType::OpenPopup:
            case ActionType::ClosePopup: {
                std::string t = a.params["target"].asString();
                if (t.empty()) add(r, Severity::Error, "action.target.required", "Action requires a popup target.", a.id);
                else {
                    auto it = byId.find(t);
                    if (it == byId.end()) add(r, Severity::Error, "action.target.missing", "Target popup '" + t + "' does not exist.", a.id);
                    else if (it->second->type != ElementType::Popup && a.type == ActionType::OpenPopup)
                        add(r, Severity::Error, "action.target.type", "OpenPopup target must be a Popup.", a.id);
                }
                break;
            }
            case ActionType::OpenScreen: {
                std::string t = a.params["target"].asString();
                if (t.empty() || !p.findScreen(t))
                    add(r, Severity::Error, "action.target.missing", "Target screen does not exist.", a.id);
                break;
            }
            case ActionType::ChangeLabelText: {
                std::string t = a.params["target"].asString();
                auto it = byId.find(t);
                if (it == byId.end()) add(r, Severity::Error, "action.target.missing", "Target label '" + t + "' does not exist.", a.id);
                else if (it->second->type != ElementType::Label)
                    add(r, Severity::Error, "action.target.type", "ChangeLabelText target must be a Label.", a.id);
                if (a.params["text"].isNull())
                    add(r, Severity::Warning, "action.text", "ChangeLabelText has no replacement text.", a.id);
                break;
            }
            case ActionType::ShowElement:
            case ActionType::HideElement:
            case ActionType::ToggleVisibility:
            case ActionType::SetEnabled:
            case ActionType::SetOpacity: {
                std::string t = a.params["target"].asString();
                if (t.empty() || !byId.count(t))
                    add(r, Severity::Error, "action.target.missing", "Action target element does not exist.", a.id);
                if (a.type == ActionType::SetOpacity) {
                    double o = a.params["value"].asNumber(-1.0);
                    if (o < 0.0 || o > 1.0)
                        add(r, Severity::Error, "action.opacity.range", "Opacity must be between 0 and 1.", a.id);
                }
                break;
            }
            case ActionType::SetValue:
            case ActionType::SaveSetting: {
                std::string sid = a.params["setting"].asString();
                if (sid.empty() || !p.findSetting(sid))
                    add(r, Severity::Error, "action.setting.missing", "Referenced project setting does not exist.", a.id);
                break;
            }
            case ActionType::Notify: {
                if (a.params["text"].isNull())
                    add(r, Severity::Error, "action.text", "Notification must have text.", a.id);
                break;
            }
            case ActionType::RunAction: {
                std::string t = a.params["action"].asString();
                if (t.empty() || !p.findAction(t))
                    add(r, Severity::Error, "action.action.missing", "Referenced action does not exist.", a.id);
                break;
            }
            case ActionType::None: break;
        }
    }

    // RunAction cycles would loop forever at runtime; detect them.
    {
        std::unordered_map<std::string, std::string> next;
        for (const auto& a : p.actions) {
            if (a.type == ActionType::RunAction) {
                std::string t = a.params["action"].asString();
                if (!t.empty()) next[a.id] = t;
            }
        }
        for (const auto& a : p.actions) {
            std::unordered_set<std::string> seen;
            std::string cur = a.id;
            while (next.count(cur)) {
                if (!seen.insert(cur).second) {
                    add(r, Severity::Error, "action.cycle",
                        "RunAction chain contains a cycle and would loop forever.", a.id);
                    break;
                }
                cur = next[cur];
            }
        }
    }

    // --- Assets -------------------------------------------------------------
    for (const auto& ast : p.assets) {
        if (ast.relativePath.empty())
            add(r, Severity::Error, "asset.path", "Asset has no relative path.", ast.id);
        else {
            // Path traversal guard.
            if (ast.relativePath.find("..") != std::string::npos ||
                ast.relativePath[0] == '/' || ast.relativePath[0] == '\\')
                add(r, Severity::Error, "asset.path.traversal",
                    "Asset path must be relative and must not contain '..'.", ast.id);
        }
    }

    return r;
}

} // namespace gmb
