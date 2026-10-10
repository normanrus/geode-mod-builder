#include "ui/ElementTree.hpp"

#include "core/ProjectValidator.hpp"
#include "ui/Ui.hpp"

#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>

using namespace geode::prelude;

namespace gmb {

bool ElementTree::init(EditorState* state, std::function<void(const std::string&)> commit) {
    if (!CCNode::init()) return false;
    m_state = state;
    m_commit = std::move(commit);

    this->setContentSize({220.f, 240.f});

    auto* scroll = ScrollLayer::create({220.f, 240.f});
    scroll->setPosition({0.f, 0.f});
    this->addChild(scroll);
    m_list = scroll->m_contentLayer;
    return true;
}

ElementTree* ElementTree::create(EditorState* state, std::function<void(const std::string&)> commit) {
    auto* ret = new ElementTree();
    if (ret->init(state, std::move(commit))) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

// Depth of an element == number of ancestors.
static int depthOf(const Project& p, const Element& e) {
    int d = 0;
    std::string cur = e.parent;
    int guard = 0;
    while (!cur.empty() && guard++ < 1000) {
        auto* par = p.findElement(cur);
        if (!par) break;
        d++;
        cur = par->parent;
    }
    return d;
}

// Ordered, flattened display list (parents immediately followed by children).
static void flatten(const Project& p, const std::vector<std::string>& roots, int depth,
                    std::vector<std::pair<std::string, int>>& out) {
    for (const auto& id : roots) {
        auto* e = p.findElement(id);
        if (!e) continue;
        out.push_back({id, depth});
        flatten(p, e->children, depth + 1, out);
    }
}

void ElementTree::refresh() {
    if (!m_list) return;
    m_list->removeAllChildren();
    const Project& p = m_state->project;

    std::vector<std::pair<std::string, int>> rows;
    for (const auto& s : p.screens) flatten(p, s.roots, 0, rows);
    // Elements that are not connected to any screen root still need to be
    // visible so the user can fix them.
    for (const auto& e : p.elements) {
        bool found = false;
        for (auto& r : rows) if (r.first == e.id) { found = true; break; }
        if (!found) rows.push_back({e.id, depthOf(p, e)});
    }

    const float rowH = 24.f;
    float total = std::max(1.f, static_cast<float>(rows.size()) * rowH + 8.f);
    m_list->setContentSize({220.f, total});

    // Build an element id -> has-error set from validation.
    std::vector<std::string> errorIds;
    if (m_validation)
        for (const auto& d : m_validation->diagnostics)
            if (d.severity == Severity::Error && !d.elementId.empty())
                errorIds.push_back(d.elementId);

    float y = total - rowH / 2.f - 4.f;
    for (auto& [id, depth] : rows) {
        auto* e = p.findElement(id);
        if (!e) continue;

        bool selected = (id == m_state->selectedId);
        std::string label = std::string(depth * 2, ' ') + e->name + "  [" + elementTypeName(e->type) + "]";
        bool hasError = std::find(errorIds.begin(), errorIds.end(), id) != errorIds.end();
        if (hasError) label += "  (!)";

        auto* row = CCMenu::create();
        row->setPosition({0.f, 0.f});
        m_list->addChild(row);

        auto* btn = makeButton(label, [this, id]() {
            m_state->selectedId = id;
            if (m_commit) m_commit("Select");
        }, 200.f, 0.4f);
        btn->setPosition({110.f, y});
        if (selected || hasError) {
            if (auto* rgba = typeinfo_cast<cocos2d::CCNodeRGBA*>(btn->getDisplayNode()))
                rgba->setColor(selected ? cocos2d::ccColor3B{120, 180, 255} : cocos2d::ccColor3B{255, 120, 120});
        }
        row->addChild(btn);

        y -= rowH;
    }
}

void ElementTree::addElement(ElementType type) {
    Project& p = m_state->project;
    Element e;
    e.id = p.newId("el");
    e.type = type;
    e.name = std::string(elementTypeName(type)) + " " + std::to_string(p.elements.size() + 1);

    // Default size / value per type so generated code is sensible.
    switch (type) {
        case ElementType::Label: e.props["text"] = std::string("Label"); break;
        case ElementType::Button: e.props["text"] = std::string("Button"); break;
        case ElementType::Slider: e.props["min"] = 0.0; e.props["max"] = 1.0; e.props["value"] = 0.5; break;
        case ElementType::TextInput: e.props["placeholder"] = std::string("Type here..."); e.width = 160.f; break;
        case ElementType::Popup: e.width = 300.f; e.height = 200.f; e.props["title"] = std::string("Popup"); break;
        case ElementType::Panel: e.width = 200.f; e.height = 120.f; break;
        default: break;
    }

    // Nest into the currently selected container when possible, else into the
    // first screen as a top level element.
    bool parented = false;
    if (auto* sel = p.findElement(m_state->selectedId)) {
        if (isContainerType(sel->type)) {
            e.parent = sel->id;
            sel->children.push_back(e.id);
            parented = true;
        }
    }
    if (!parented && type != ElementType::Popup) {
        if (!p.screens.empty()) {
            p.screens.front().roots.push_back(e.id);
        } else if (!p.elements.empty()) {
            e.parent = p.elements.front().id;
            p.elements.front().children.push_back(e.id);
        }
    }
    // Popups are kept out of the tree; they are opened by actions.

    p.elements.push_back(e);
    m_state->selectedId = e.id;
    if (m_commit) m_commit("Add " + std::string(elementTypeName(type)));
}

void ElementTree::renameSelected() {
    Project& p = m_state->project;
    auto* e = p.findElement(m_state->selectedId);
    if (!e) return;

    std::string id = e->id;
    std::string current = e->name;
    createQuickPopup("Rename Element",
        "Use the Name field in the properties panel to rename this element.",
        "Cancel", "Edit Name",
        [this, id](FLAlertLayer*, bool btn2) {
            if (!btn2) return;
            // Focus is handled by the properties panel; select and refresh.
            m_state->selectedId = id;
            if (m_commit) m_commit("Rename element");
        });
    (void)current;
}

void ElementTree::duplicateSelected() {
    Project& p = m_state->project;
    auto* e = p.findElement(m_state->selectedId);
    if (!e) return;

    Element copy = *e;
    copy.id = p.newId("el");
    copy.name = e->name + " copy";
    copy.children.clear();
    p.elements.push_back(copy);

    // Insert right after the original in its parent's child list / screen roots.
    auto insertAfter = [&](std::vector<std::string>& list) {
        auto it = std::find(list.begin(), list.end(), e->id);
        if (it != list.end()) list.insert(it + 1, copy.id);
        else list.push_back(copy.id);
    };
    if (!copy.parent.empty()) {
        if (auto* par = p.findElement(copy.parent)) insertAfter(par->children);
    } else {
        for (auto& s : p.screens) {
            if (std::find(s.roots.begin(), s.roots.end(), e->id) != s.roots.end()) {
                insertAfter(s.roots);
                break;
            }
        }
    }
    m_state->selectedId = copy.id;
    if (m_commit) m_commit("Duplicate element");
}

void ElementTree::deleteSelected() {
    Project& p = m_state->project;
    std::string id = m_state->selectedId;
    auto* e = p.findElement(id);
    if (!e) return;

    // Recursively collect descendants.
    std::vector<std::string> toRemove;
    std::function<void(const std::string&)> collect = [&](const std::string& cur) {
        toRemove.push_back(cur);
        auto* el = p.findElement(cur);
        if (!el) return;
        for (const auto& c : el->children) collect(c);
    };
    collect(id);

    // Confirm, because deletion is destructive.
    createQuickPopup("Delete Element", "Delete this element and its children?", "Cancel", "Delete",
        [this, id, toRemove](FLAlertLayer*, bool btn2) {
            if (!btn2) return;
            Project& proj = m_state->project;
            // Unparent from the parent's child list / screen roots.
            if (auto* el = proj.findElement(id)) {
                if (!el->parent.empty()) {
                    if (auto* par = proj.findElement(el->parent))
                        par->children.erase(std::remove(par->children.begin(), par->children.end(), id), par->children.end());
                } else {
                    for (auto& s : proj.screens)
                        s.roots.erase(std::remove(s.roots.begin(), s.roots.end(), id), s.roots.end());
                }
            }
            // Remove elements.
            proj.elements.erase(std::remove_if(proj.elements.begin(), proj.elements.end(),
                [&](const Element& x) {
                    return std::find(toRemove.begin(), toRemove.end(), x.id) != toRemove.end();
                }), proj.elements.end());
            // Remove actions owned by removed elements.
            proj.actions.erase(std::remove_if(proj.actions.begin(), proj.actions.end(),
                [&](const Action& a) {
                    std::string owner = a.params["owner"].asString();
                    return std::find(toRemove.begin(), toRemove.end(), owner) != toRemove.end();
                }), proj.actions.end());
            m_state->selectedId.clear();
            if (m_commit) m_commit("Delete element");
        });
}

// Reorders within the parent's child list (or screen roots).
void ElementTree::moveSelected(int delta) {
    Project& p = m_state->project;
    auto* e = p.findElement(m_state->selectedId);
    if (!e) return;

    auto moveIn = [&](std::vector<std::string>& list) {
        auto it = std::find(list.begin(), list.end(), e->id);
        if (it == list.end()) return false;
        long idx = it - list.begin();
        long target = idx + delta;
        if (target < 0 || target >= static_cast<long>(list.size())) return false;
        std::iter_swap(list.begin() + idx, list.begin() + target);
        return true;
    };

    bool moved = false;
    if (!e->parent.empty()) {
        if (auto* par = p.findElement(e->parent)) moved = moveIn(par->children);
    } else {
        for (auto& s : p.screens) if (moveIn(s.roots)) { moved = true; break; }
    }
    if (moved && m_commit) m_commit("Reorder element");
}

// Moves the selected element into the first container found upwards.
void ElementTree::nestSelected() {
    Project& p = m_state->project;
    auto* e = p.findElement(m_state->selectedId);
    if (!e) return;

    // Find a container that is not a descendant of `e`.
    for (auto& candidate : p.elements) {
        if (candidate.id == e->id) continue;
        if (!(isContainerType(candidate.type))) continue;
        // Reject if candidate is a descendant of e (would create a cycle).
        bool descendant = false;
        std::string cur = candidate.parent;
        int guard = 0;
        while (!cur.empty() && guard++ < 1000) {
            if (cur == e->id) { descendant = true; break; }
            auto* par = p.findElement(cur);
            if (!par) break;
            cur = par->parent;
        }
        if (descendant) continue;

        // Unlink from old parent / screen.
        if (!e->parent.empty()) {
            if (auto* oldPar = p.findElement(e->parent))
                oldPar->children.erase(std::remove(oldPar->children.begin(), oldPar->children.end(), e->id), oldPar->children.end());
        } else {
            for (auto& s : p.screens)
                s.roots.erase(std::remove(s.roots.begin(), s.roots.end(), e->id), s.roots.end());
        }
        e->parent = candidate.id;
        candidate.children.push_back(e->id);
        if (m_commit) m_commit("Nest element");
        return;
    }
}

void ElementTree::selectParent() {
    Project& p = m_state->project;
    auto* e = p.findElement(m_state->selectedId);
    if (!e || e->parent.empty()) return;
    m_state->selectedId = e->parent;
    if (m_commit) m_commit("Select parent");
}

} // namespace gmb
