#include "ui/ActionEditor.hpp"

#include "ui/Ui.hpp"

#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>

using namespace geode::prelude;

namespace gmb {

bool ActionEditor::init(EditorState* state, std::function<void(const std::string&)> commit, std::function<void()> onChanged) {
    if (!Popup::init(440.f, 340.f)) return false;
    m_state = state;
    m_commit = std::move(commit);
    m_onChanged = std::move(onChanged);
    this->setTitle("Actions & Events");

    // Add-action picker: a reliable list of choices (not a node graph).
    struct Choice { const char* label; ActionType type; EventType event; };
    static const Choice choices[] = {
        {"Open Popup", ActionType::OpenPopup, EventType::OnClick},
        {"Close Popup", ActionType::ClosePopup, EventType::OnClick},
        {"Open Screen", ActionType::OpenScreen, EventType::OnClick},
        {"Change Label", ActionType::ChangeLabelText, EventType::OnClick},
        {"Show Element", ActionType::ShowElement, EventType::OnClick},
        {"Hide Element", ActionType::HideElement, EventType::OnClick},
        {"Toggle Visible", ActionType::ToggleVisibility, EventType::OnClick},
        {"Set Enabled", ActionType::SetEnabled, EventType::OnValueChanged},
        {"Set Opacity", ActionType::SetOpacity, EventType::OnClick},
        {"Set Setting", ActionType::SetValue, EventType::OnValueChanged},
        {"Save Setting", ActionType::SaveSetting, EventType::OnClick},
        {"Notify", ActionType::Notify, EventType::OnClick},
        {"Run Action", ActionType::RunAction, EventType::OnClick},
    };

    auto* picker = CCMenu::create();
    picker->setPosition({0.f, 72.f});
    this->addChild(picker, 10);
    for (const auto& c : choices) {
        if (auto* b = makeButton(c.label, [this, c]() { this->addAction(c.type, c.event); }, 0.35f))
            picker->addChild(b);
    }
    picker->alignItemsInColumns(4, 4, 4, 1, 0);

    auto* listTitle = makeLabel("Actions on the selected element", "bigFont.fnt", 0.4f);
    listTitle->setPosition({0.f, -14.f});
    this->addChild(listTitle, 10);

    auto* scroll = ScrollLayer::create({410.f, 108.f});
    scroll->setPosition({-205.f, -158.f});
    this->addChild(scroll, 10);
    m_list = scroll->m_contentLayer;

    this->rebuild();
    return true;
}

ActionEditor* ActionEditor::create(EditorState* state, std::function<void(const std::string&)> commit,
                                   std::function<void()> onChanged) {
    auto* ret = new ActionEditor();
    if (ret->init(state, std::move(commit), std::move(onChanged))) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

void ActionEditor::addAction(ActionType type, EventType event) {
    std::string owner = m_state->selectedId;
    if (owner.empty()) {
        FLAlertLayer::create("Actions", "Select an element first.", "OK")->show();
        return;
    }
    Action a;
    a.id = m_state->project.newId("act");
    a.type = type;
    a.event = event;
    a.params["owner"] = owner;
    // Sensible defaults for parameterized actions.
    if (type == ActionType::Notify) a.params["text"] = std::string("Hello!");
    if (type == ActionType::SetOpacity) a.params["value"] = 1.0;
    if (type == ActionType::SetEnabled) a.params["value"] = true;
    if (type == ActionType::ChangeLabelText) a.params["text"] = std::string("New text");
    m_state->project.actions.push_back(a);
    if (m_commit) m_commit("Add action");
    if (m_onChanged) m_onChanged();
    this->rebuild();
}

static std::vector<std::pair<std::string, std::string>> targetOptions(const Project& p, ActionType type) {
    std::vector<std::pair<std::string, std::string>> out; // (id, label)
    for (const auto& e : p.elements) {
        bool ok = false;
        switch (type) {
            case ActionType::OpenPopup:
            case ActionType::ClosePopup: ok = (e.type == ElementType::Popup); break;
            case ActionType::ChangeLabelText: ok = (e.type == ElementType::Label); break;
            case ActionType::ShowElement:
            case ActionType::HideElement:
            case ActionType::ToggleVisibility:
            case ActionType::SetEnabled:
            case ActionType::SetOpacity: ok = true; break;
            default: break;
        }
        if (ok) out.push_back({e.id, e.name + " [" + elementTypeName(e.type) + "]"});
    }
    return out;
}

void ActionEditor::openParams(Action& a, size_t index) {
    // A compact popup with one field describing the needed parameter, plus a
    // target picker that cycles through valid targets.
    std::string title = actionTypeName(a.type);
    std::string body;
    std::string actionId = a.id;
    EditorState* state = m_state;
    auto commit = m_commit;
    auto rebuildCb = [this]() { this->rebuild(); };

    if (a.type == ActionType::OpenPopup || a.type == ActionType::ClosePopup ||
        a.type == ActionType::ChangeLabelText || a.type == ActionType::ShowElement ||
        a.type == ActionType::HideElement || a.type == ActionType::ToggleVisibility ||
        a.type == ActionType::SetEnabled || a.type == ActionType::SetOpacity) {
        body = "Target: " + a.params["target"].asString("(none)") + "\nTap OK to cycle target.";
    } else if (a.type == ActionType::OpenScreen) {
        body = "Target screen: " + a.params["target"].asString("(none)") + "\nTap OK to cycle screen.";
    } else if (a.type == ActionType::SaveSetting || a.type == ActionType::SetValue) {
        body = "Setting: " + a.params["setting"].asString("(none)") + "\nTap OK to cycle setting.";
    } else if (a.type == ActionType::RunAction) {
        body = "Action: " + a.params["action"].asString("(none)") + "\nTap OK to cycle action.";
    } else {
        body = "This action has no target parameter.";
    }

    createQuickPopup(title.c_str(), body.c_str(), "Cancel", "OK",
        [state, actionId, rebuildCb, commit](FLAlertLayer*, bool btn2) {
            if (!btn2) return;
            auto* act = state->project.findAction(actionId);
            if (!act) return;
            if (act->type == ActionType::OpenPopup || act->type == ActionType::ClosePopup ||
                act->type == ActionType::ChangeLabelText || act->type == ActionType::ShowElement ||
                act->type == ActionType::HideElement || act->type == ActionType::ToggleVisibility ||
                act->type == ActionType::SetEnabled || act->type == ActionType::SetOpacity) {
                auto opts = targetOptions(state->project, act->type);
                if (opts.empty()) return;
                std::string cur = act->params["target"].asString();
                size_t idx = 0;
                for (size_t k = 0; k < opts.size(); k++) if (opts[k].first == cur) idx = k;
                idx = (idx + 1) % opts.size();
                act->params["target"] = opts[idx].first;
            } else if (act->type == ActionType::OpenScreen) {
                if (state->project.screens.empty()) return;
                std::string cur = act->params["target"].asString();
                size_t idx = 0;
                for (size_t k = 0; k < state->project.screens.size(); k++)
                    if (state->project.screens[k].id == cur) idx = k;
                idx = (idx + 1) % state->project.screens.size();
                act->params["target"] = state->project.screens[idx].id;
            } else if (act->type == ActionType::SaveSetting || act->type == ActionType::SetValue) {
                if (state->project.settings.empty()) {
                    // Create a setting on demand so the action is usable.
                    ProjectSetting s;
                    s.id = state->project.newId("set");
                    s.key = "setting" + std::to_string(state->project.settings.size() + 1);
                    s.type = "string";
                    s.value = "";
                    state->project.settings.push_back(s);
                }
                std::string cur = act->params["setting"].asString();
                size_t idx = 0;
                for (size_t k = 0; k < state->project.settings.size(); k++)
                    if (state->project.settings[k].id == cur) idx = k;
                idx = (idx + 1) % state->project.settings.size();
                act->params["setting"] = state->project.settings[idx].id;
            } else if (act->type == ActionType::RunAction) {
                std::vector<std::string> ids;
                for (auto& x : state->project.actions) if (x.id != actionId) ids.push_back(x.id);
                if (ids.empty()) return;
                std::string cur = act->params["action"].asString();
                size_t idx = 0;
                for (size_t k = 0; k < ids.size(); k++) if (ids[k] == cur) idx = k;
                idx = (idx + 1) % ids.size();
                act->params["action"] = ids[idx];
            }
            if (commit) commit("Edit action parameters");
            rebuildCb();
        });
}

void ActionEditor::rebuild() {
    if (!m_list) return;
    m_list->removeAllChildren();

    std::string owner = m_state->selectedId;
    std::vector<size_t> indices;
    for (size_t i = 0; i < m_state->project.actions.size(); i++)
        if (m_state->project.actions[i].params["owner"].asString() == owner) indices.push_back(i);

    m_list->setContentSize({410.f, std::max(1.f, indices.size() * 30.f + 8.f)});

    if (owner.empty()) {
        auto* l = makeLabel("No element selected.", "bigFont.fnt", 0.4f);
        l->setPosition({205.f, 20.f});
        m_list->addChild(l);
        return;
    }
    if (indices.empty()) {
        auto* l = makeLabel("No actions yet. Add one above.", "bigFont.fnt", 0.4f);
        l->setPosition({205.f, 20.f});
        m_list->addChild(l);
        return;
    }

    float y = m_list->getContentSize().height - 18.f;
    for (size_t i : indices) {
        Action& a = m_state->project.actions[i];
        std::string label = std::string(eventTypeName(a.event)) + ": " + actionTypeName(a.type) + " (" + a.id + ")";
        auto* row = CCMenu::create();
        m_list->addChild(row);

        auto* l = makeLabel(label, "bigFont.fnt", 0.35f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setPosition({6.f, y});
        m_list->addChild(l);

        std::string aid = a.id;
        size_t idx = i;
        auto* paramsBtn = makeButton("Params", [this, idx]() {
            if (idx < m_state->project.actions.size()) this->openParams(m_state->project.actions[idx], idx);
        }, 0.32f);
        paramsBtn->setPosition({300.f, y});
        row->addChild(paramsBtn);

        auto* delBtn = makeButton("X", [this, aid]() {
            auto& acts = m_state->project.actions;
            acts.erase(std::remove_if(acts.begin(), acts.end(),
                [&](const Action& x) { return x.id == aid; }), acts.end());
            if (m_commit) m_commit("Delete action");
            if (m_onChanged) m_onChanged();
            this->rebuild();
        }, 0.32f);
        delBtn->setPosition({355.f, y});
        row->addChild(delBtn);

        y -= 30.f;
    }
}

} // namespace gmb
