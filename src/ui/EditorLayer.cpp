#include "ui/EditorLayer.hpp"

#include "core/BuildManager.hpp"
#include "core/CodeGenerator.hpp"
#include "core/ProjectValidator.hpp"
#include "ui/BuilderLayer.hpp"
#include "ui/ActionEditor.hpp"
#include "ui/Ui.hpp"

#include <Geode/ui/ScrollLayer.hpp>
#include <ctime>

using namespace geode::prelude;

namespace gmb {

bool EditorLayer::init() {
    if (!CCLayer::init()) return false;
    if (!m_state) return false;

    auto win = CCDirector::sharedDirector()->getWinSize();
    const float W = win.width;
    const float H = win.height;

    // Full-screen dark backdrop, kept behind everything else.
    auto* bg = makePanel({W, H}, {16, 16, 20}, 255);
    if (bg) {
        bg->setAnchorPoint({0.f, 0.f});
        bg->setPosition({0.f, 0.f});
        this->addChild(bg, -10);
    }

    m_title = makeLabel("", "goldFont.fnt", 0.45f);
    m_title->setAnchorPoint({0.f, 0.5f});
    m_title->setPosition({8.f, H - 10.f});
    this->addChild(m_title);

    // Toolkit rows use CCMenu + alignItemsHorizontally so items are evenly
    // spaced regardless of label width, at the fixed GD design resolution.
    auto mkRow = [&](float y, float scale) {
        auto* m = CCMenu::create();
        m->setPosition({W / 2.f, y});
        m->setScale(scale);
        this->addChild(m, 10);
        return m;
    };
    auto addItem = [](CCMenu* m, const char* label, std::function<void()> cb) {
        if (auto* b = makeButton(label, std::move(cb), 0.5f)) m->addChild(b);
    };

    auto* row1 = mkRow(H - 12.f, 0.34f);
    addItem(row1, "Back",    [this]() { this->onBack(); });
    addItem(row1, "Save",    [this]() { this->onSave(); });
    addItem(row1, "Save As", [this]() { this->onSaveAs(); });
    addItem(row1, "Undo",    [this]() { this->onUndo(); });
    addItem(row1, "Redo",    [this]() { this->onRedo(); });
    row1->alignItemsHorizontallyWithPadding(2.f);

    auto* row2 = mkRow(H - 30.f, 0.34f);
    addItem(row2, "New",      [this]() { this->onNewElement(); });
    addItem(row2, "Generate", [this]() { this->onGenerate(); });
    addItem(row2, "Build",    [this]() { this->onBuild(); });
    addItem(row2, "Actions",  [this]() { this->onActions(); });
    addItem(row2, "Preview",  [this]() { this->onPreviewPopup(); });
    row2->alignItemsHorizontallyWithPadding(2.f);

    // Element palette: adds an element of the given type (see onNewElementType).
    struct Pal { const char* label; ElementType type; };
    static const Pal pals[] = {
        {"Button", ElementType::Button}, {"Label", ElementType::Label},
        {"Sprite", ElementType::Sprite}, {"Panel", ElementType::Panel},
        {"Popup", ElementType::Popup}, {"Checkbox", ElementType::Checkbox},
        {"Slider", ElementType::Slider}, {"Input", ElementType::TextInput},
        {"Close", ElementType::CloseButton},
    };
    auto* palette = CCMenu::create();
    palette->setPosition({W / 2.f, H - 48.f});
    palette->setScale(0.34f);
    this->addChild(palette, 10);
    for (const auto& p : pals)
        if (auto* b = makeButton(p.label, [this, t = p.type]() { this->onNewElementType(t); }, 0.5f))
            palette->addChild(b);
    palette->alignItemsHorizontallyWithPadding(2.f);

    // Element tree operations.
    auto* treeOps = CCMenu::create();
    treeOps->setPosition({W / 2.f, H - 66.f});
    treeOps->setScale(0.32f);
    this->addChild(treeOps, 10);
    addItem(treeOps, "Up",     [this]() { m_tree->moveSelected(-1); });
    addItem(treeOps, "Down",   [this]() { m_tree->moveSelected(1); });
    addItem(treeOps, "Dup",    [this]() { m_tree->duplicateSelected(); });
    addItem(treeOps, "Nest",   [this]() { m_tree->nestSelected(); });
    addItem(treeOps, "Parent", [this]() { m_tree->selectParent(); });
    addItem(treeOps, "Del",    [this]() { m_tree->deleteSelected(); });
    treeOps->alignItemsHorizontallyWithPadding(2.f);

    // Canvas operations.
    auto* canvasOps = CCMenu::create();
    canvasOps->setPosition({W / 2.f, H - 84.f});
    canvasOps->setScale(0.32f);
    this->addChild(canvasOps, 10);
    addItem(canvasOps, "Z+",   [this]() { m_canvas->setZoom(m_canvas->zoom() + 0.25f); });
    addItem(canvasOps, "Z-",   [this]() { m_canvas->setZoom(m_canvas->zoom() - 0.25f); });
    addItem(canvasOps, "Snap", [this]() { m_canvas->toggleSnap(); notify(m_canvas->snap() ? "Snap on" : "Snap off"); });
    addItem(canvasOps, "L",    [this]() { m_canvas->alignSelected(0); });
    addItem(canvasOps, "R",    [this]() { m_canvas->alignSelected(1); });
    addItem(canvasOps, "T",    [this]() { m_canvas->alignSelected(2); });
    addItem(canvasOps, "B",    [this]() { m_canvas->alignSelected(3); });
    addItem(canvasOps, "H",    [this]() { m_canvas->alignSelected(4); });
    addItem(canvasOps, "V",    [this]() { m_canvas->alignSelected(5); });
    canvasOps->alignItemsHorizontallyWithPadding(2.f);

    // Three columns: tree | canvas | properties.
    const float colTop = H - 100.f;
    const float colBottom = 22.f;
    const float colH = colTop - colBottom;

    const float treeW = 112.f;
    const float canvasX = 4.f + treeW + 4.f;
    const float propsW = 196.f;
    const float canvasW = W - canvasX - propsW - 8.f;
    const float propsX = canvasX + canvasW + 4.f;

    m_tree = ElementTree::create(m_state.get(), [this](const std::string& l) { this->commitAndRefresh(l); }, {treeW, colH});
    m_tree->setPosition({6.f, colBottom});
    this->addChild(m_tree, 1);

    m_canvas = CanvasPreview::create(m_state.get(), [this](const std::string& l) { this->commitAndRefresh(l); }, {canvasW, colH});
    m_canvas->setPosition({canvasX, colBottom});
    this->addChild(m_canvas, 1);

    m_props = PropertiesPanel::create(m_state.get(), [this](const std::string& l) { this->commitAndRefresh(l); }, {propsW, colH});
    m_props->setPosition({propsX, colBottom});
    this->addChild(m_props, 1);

    // Status bar.
    m_status = makeLabel("", "bigFont.fnt", 0.32f);
    m_status->setAnchorPoint({0.f, 0.5f});
    m_status->setPosition({8.f, 11.f});
    this->addChild(m_status);

    this->rebuildAll();
    this->setKeypadEnabled(true);
    return true;
}

void EditorLayer::commitAndRefresh(const std::string& label) {
    if (label != "Select") {
        m_state->commit(label);
        // Refresh validation and status.
    }
    m_state->project.meta.modifiedAt = "";
    this->rebuildAll();
}

void EditorLayer::rebuildAll() {
    // Re-run validation once and share it.
    auto vr = std::make_shared<ValidationResult>(ProjectValidator::validate(m_state->project));
    if (m_tree) { m_tree->setValidation(vr); m_tree->refresh(); }
    if (m_canvas) m_canvas->refresh();
    if (m_props) m_props->refresh();
    this->updateTitleAndStatus();
}

void EditorLayer::updateTitleAndStatus() {
    if (m_title) {
        std::string name = m_state->project.meta.name.empty() ? m_state->fileName : m_state->project.meta.name;
        m_title->setString((name + (m_state->dirty ? " *" : "")).c_str());
    }
    if (m_status) {
        auto vr = ProjectValidator::validate(m_state->project);
        std::string sel = m_state->selectedId.empty() ? "none" : m_state->selectedId;
        std::string s = "Selected: " + sel +
                        "   Elements: " + std::to_string(m_state->project.elements.size()) +
                        "   Actions: " + std::to_string(m_state->project.actions.size()) +
                        "   Errors: " + std::to_string(vr.errorCount()) +
                        "   Warnings: " + std::to_string(vr.warningCount());
        m_status->setString(s.c_str());
    }
}

void EditorLayer::keyBackClicked() { this->onBack(); }
void EditorLayer::onExit() { CCLayer::onExit(); }

void EditorLayer::onBack() {
    if (m_state->dirty) {
        createQuickPopup("Unsaved Changes", "Save before closing?", "Discard", "Save",
            [this](FLAlertLayer*, bool save) {
                if (save) this->onSave();
                CCDirector::sharedDirector()->popScene();
            });
    } else {
        CCDirector::sharedDirector()->popScene();
    }
}

void EditorLayer::onSave() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    m_state->project.meta.modifiedAt = buf;

    std::string err;
    if (m_state->manager->saveProject(m_state->project, m_state->fileName, err)) {
        m_state->dirty = false;
        notify("Project saved.", NotificationIcon::Success);
        this->updateTitleAndStatus();
    } else {
        notify("Save failed: " + err, NotificationIcon::Error);
    }
}

void EditorLayer::onSaveAs() {
    // Save As writes to a NEW file name so the original project is preserved.
    // A numeric suffix is appended until an unused name is found.
    std::string base = m_state->project.meta.name.empty() ? m_state->project.meta.modId : m_state->project.meta.name;
    std::string stem = base;
    if (stem.size() > 4 && stem.substr(stem.size() - 5) == ".json") stem.resize(stem.size() - 5);

    std::string used;
    std::string err;
    bool saved = false;
    for (int i = 1; i <= 50 && !saved; i++) {
        std::string candidate = stem + "_copy" + (i == 1 ? "" : std::to_string(i));
        if (m_state->manager->saveProjectAs(m_state->project, candidate, used, err)) {
            // saveProjectAs never fails to pick the requested name; guard against
            // silently overwriting the currently open file.
            if (used != m_state->fileName) saved = true;
        }
    }
    if (!saved) {
        notify("Save As failed: " + err, NotificationIcon::Error);
        return;
    }
    m_state->fileName = used;
    m_state->dirty = false;
    notify("Saved as " + used, NotificationIcon::Success);
    this->updateTitleAndStatus();
}

void EditorLayer::onUndo() {
    Project out;
    if (m_state->undo->undo(out)) {
        m_state->project = std::move(out);
        m_state->dirty = true;
        this->rebuildAll();
        notify("Undo: " + m_state->undo->redoLabel());
    } else {
        notify("Nothing to undo.", NotificationIcon::Warning);
    }
}

void EditorLayer::onRedo() {
    Project out;
    if (m_state->undo->redo(out)) {
        m_state->project = std::move(out);
        m_state->dirty = true;
        this->rebuildAll();
        notify("Redo");
    } else {
        notify("Nothing to redo.", NotificationIcon::Warning);
    }
}

void EditorLayer::onGenerate() {
    auto vr = ProjectValidator::validate(m_state->project);
    if (vr.hasErrors()) {
        std::string msg = "Fix these errors before generating:\n";
        int shown = 0;
        for (const auto& d : vr.diagnostics) {
            if (d.severity != Severity::Error) continue;
            msg += "- " + d.message + "\n";
            if (++shown >= 10) break;
        }
        FLAlertLayer::create("Validation failed", msg.c_str(), "OK")->show();
        return;
    }

    auto gen = CodeGenerator::generate(m_state->project);
    if (!gen.ok) {
        std::string msg = "Generation failed:\n";
        for (const auto& d : gen.diagnostics)
            if (d.severity == Severity::Error) msg += "- " + d.message + "\n";
        FLAlertLayer::create("Generation failed", msg.c_str(), "OK")->show();
        return;
    }

    std::string dirName = m_state->project.meta.name.empty() ? m_state->project.meta.modId : m_state->project.meta.name;
    auto report = m_state->manager->writeGenerated(m_state->project, gen, dirName);
    if (!report.ok) {
        FLAlertLayer::create("Export failed", report.message.c_str(), "OK")->show();
        return;
    }

    std::string msg = report.message + "\n\nFiles:\n";
    for (const auto& f : report.writtenFiles) msg += "  " + f + "\n";
    msg += "\nOutput:\n" + report.outputRoot + "\n\n";
    msg += "Build instructions:\n";
    auto steps = BuildManager::instructions(report.outputRoot, m_state->project.meta);
    int n = 0;
    for (const auto& s : steps) { if (++n > 4) break; msg += s + "\n"; }
    FLAlertLayer::create("Generate Code", msg.c_str(), "OK")->show();
}

void EditorLayer::onBuild() {
    // Regenerate first so we always build current code.
    auto gen = CodeGenerator::generate(m_state->project);
    if (!gen.ok) { FLAlertLayer::create("Build", "Fix validation errors first.", "OK")->show(); return; }
    std::string dirName = m_state->project.meta.name.empty() ? m_state->project.meta.modId : m_state->project.meta.name;
    auto report = m_state->manager->writeGenerated(m_state->project, gen, dirName);
    if (!report.ok) { FLAlertLayer::create("Build", report.message.c_str(), "OK")->show(); return; }

    auto tc = ProjectManager::detectToolchain();
    std::string confirm = "Build the generated project now?\n\n";
    confirm += "Geode CLI: " + std::string(tc.geodeCliFound ? "found" : "missing") + "\n";
    confirm += "GEODE_SDK: " + std::string(tc.sdkFound ? "set" : "not set") + "\n\n";
    confirm += "If the toolchain is missing, the build will be skipped and instructions shown.";

    std::string outRoot = report.outputRoot;
    createQuickPopup("Build Mod", confirm, "Cancel", "Build",
        [outRoot, meta = m_state->project.meta](FLAlertLayer*, bool btn2) {
            if (!btn2) return;
            BuildResult br = BuildManager::build(outRoot, meta, true);
            std::string msg;
            if (!br.attempted) {
                msg = "Build skipped.\n\n" + br.instructions;
            } else {
                msg = "Command: " + br.command + "\nExit code: " + std::to_string(br.exitCode) + "\n\n";
                msg += BuildManager::classifyOutput(br.output) + "\n\n";
                // Show the tail of compiler output (bounded).
                std::string tail = br.output;
                if (tail.size() > 800) tail = tail.substr(tail.size() - 800);
                msg += tail;
                if (!br.success) msg += "\n\nSources were kept on disk.";
            }
            FLAlertLayer::create(br.success ? "Build succeeded" : "Build result", msg.c_str(), "OK")->show();
        });
}

void EditorLayer::onPreviewPopup() {
    Element* popup = m_state->project.findElement(m_state->selectedId);
    if (!popup || popup->type != ElementType::Popup) {
        for (auto& e : m_state->project.elements) if (e.type == ElementType::Popup) { popup = &e; break; }
    }
    if (!popup) {
        notify("No popup in this project.", NotificationIcon::Warning);
        return;
    }
    std::string title = popup->props["title"].asString(popup->name);
    std::string body = popup->props["message"].asString("(no message)");
    body += "\n\n[Editor preview only]";
    FLAlertLayer::create(title.c_str(), body.c_str(), "Close")->show();
}

void EditorLayer::onActions() {
    ActionEditor::create(m_state.get(),
        [this](const std::string& l) { this->commitAndRefresh(l); },
        [this]() { this->rebuildAll(); })->show();
}

void EditorLayer::onNewElementType(ElementType type) {
    if (m_tree) m_tree->addElement(type);
}

void EditorLayer::onNewElement() {
    // "New" opens a small dark panel of element types: a single entry point
    // that still covers every supported element.
    auto win = CCDirector::sharedDirector()->getWinSize();
    auto* panel = CCLayer::create();
    if (!panel) return;
    panel->setContentSize(win);

    // Dimmer that swallows touches behind the panel.
    auto* dim = makePanel(win, {0, 0, 0}, 150);
    if (dim) panel->addChild(dim, 0);

    CCSize psize{280.f, 176.f};
    auto* bg = makePanel(psize, {26, 26, 32}, 255);
    if (bg) {
        bg->setAnchorPoint({0.5f, 0.5f});
        bg->setPosition({win.width / 2.f, win.height / 2.f});
        panel->addChild(bg, 1);
    }
    auto* title = makeLabel("New Element", "goldFont.fnt", 0.6f);
    title->setPosition({win.width / 2.f, win.height / 2.f + 64.f});
    panel->addChild(title, 2);

    auto* menu = CCMenu::create();
    menu->setPosition({win.width / 2.f, win.height / 2.f - 6.f});
    panel->addChild(menu, 2);
    static const std::pair<const char*, ElementType> items[] = {
        {"Button", ElementType::Button}, {"Label", ElementType::Label},
        {"Sprite", ElementType::Sprite}, {"Panel", ElementType::Panel},
        {"Popup", ElementType::Popup},   {"Checkbox", ElementType::Checkbox},
        {"Slider", ElementType::Slider}, {"Text Input", ElementType::TextInput},
        {"Close Button", ElementType::CloseButton},
    };
    for (const auto& it : items) {
        auto* b = makeButton(it.first, [this, panel, t = it.second]() {
            if (m_tree) m_tree->addElement(t);
            panel->removeFromParent();
        }, 0.45f);
        if (b) menu->addChild(b);
    }
    menu->alignItemsInColumns(3, 3, 3, 0);

    auto* closeMenu = CCMenu::create();
    closeMenu->setPosition({win.width / 2.f, win.height / 2.f - 64.f});
    panel->addChild(closeMenu, 2);
    if (auto* c = makeButton("Cancel", [panel]() { panel->removeFromParent(); }, 0.5f))
        closeMenu->addChild(c);

    if (auto* scene = CCDirector::sharedDirector()->getRunningScene())
        scene->addChild(panel, 100);
}

EditorLayer* EditorLayer::create(std::shared_ptr<EditorState> state) {
    auto* ret = new EditorLayer();
    ret->m_state = std::move(state);
    if (ret->init()) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

cocos2d::CCScene* EditorLayer::scene(std::shared_ptr<EditorState> state) {
    auto* scene = CCScene::create();
    scene->addChild(EditorLayer::create(std::move(state)));
    return scene;
}

} // namespace gmb
