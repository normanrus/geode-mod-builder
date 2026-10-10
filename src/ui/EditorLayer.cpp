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

    auto* bg = makePanel(win, {16, 16, 20}, 255);
    if (bg) this->addChild(bg, -10);

    m_title = makeLabel("", "goldFont.fnt", 0.6f);
    m_title->setAnchorPoint({0.f, 0.5f});
    m_title->setPosition({10.f, win.height - 20.f});
    this->addChild(m_title);

    // --- Toolbar -----------------------------------------------------------
    auto* toolbar = CCMenu::create();
    this->addChild(toolbar, 5);

    float tx = 180.f;
    float ty = win.height - 20.f;
    auto addTool = [&](const char* label, std::function<void()> cb, float w = 70.f) {
        auto* b = makeButton(label, std::move(cb), w, 0.45f);
        b->setPosition({tx, ty});
        toolbar->addChild(b);
        tx += w + 6.f;
    };
    addTool("Back", [this]() { this->onBack(); }, 60.f);
    addTool("Save", [this]() { this->onSave(); }, 60.f);
    addTool("Save As", [this]() { this->onSaveAs(); }, 70.f);
    addTool("Undo", [this]() { this->onUndo(); }, 60.f);
    addTool("Redo", [this]() { this->onRedo(); }, 60.f);
    addTool("Generate", [this]() { this->onGenerate(); }, 80.f);
    addTool("Build", [this]() { this->onBuild(); }, 60.f);
    addTool("Actions", [this]() { this->onActions(); }, 70.f);

    // --- Left: element palette + tree -------------------------------------
    auto* palette = CCMenu::create();
    this->addChild(palette, 5);
    float px = 20.f, py = win.height - 90.f;
    struct Pal { const char* label; ElementType type; };
    static const Pal pals[] = {
        {"Button", ElementType::Button}, {"Label", ElementType::Label},
        {"Sprite", ElementType::Sprite}, {"Panel", ElementType::Panel},
        {"Popup", ElementType::Popup}, {"Checkbox", ElementType::Checkbox},
        {"Slider", ElementType::Slider}, {"Text Input", ElementType::TextInput},
        {"Close", ElementType::CloseButton},
    };
    int pi = 0;
    for (const auto& p : pals) {
        auto* b = makeButton(p.label, [this, t = p.type]() { m_tree->addElement(t); }, 100.f, 0.4f);
        b->setPosition({px, py});
        palette->addChild(b);
        px += 106.f;
        if (++pi % 2 == 0) { px = 20.f; py -= 28.f; }
    }

    m_tree = ElementTree::create(m_state.get(), [this](const std::string& l) { this->commitAndRefresh(l); });
    m_tree->setPosition({10.f, 168.f});
    this->addChild(m_tree);

    // Tree operations toolbar.
    auto* treeOps = CCMenu::create();
    this->addChild(treeOps, 5);
    float ox = 10.f;
    float oy = 168.f;
    auto addOp = [&](const char* label, std::function<void()> cb, float w = 60.f) {
        auto* b = makeButton(label, std::move(cb), w, 0.4f);
        b->setPosition({ox, oy});
        treeOps->addChild(b);
        ox += w + 4.f;
    };
    addOp("Up", [this]() { m_tree->moveSelected(-1); }, 50.f);
    addOp("Down", [this]() { m_tree->moveSelected(1); }, 56.f);
    addOp("Dup", [this]() { m_tree->duplicateSelected(); }, 50.f);
    addOp("Nest", [this]() { m_tree->nestSelected(); }, 56.f);
    addOp("Parent", [this]() { m_tree->selectParent(); }, 60.f);
    addOp("Del", [this]() { m_tree->deleteSelected(); }, 50.f);

    // --- Center: canvas ----------------------------------------------------
    m_canvas = CanvasPreview::create(m_state.get(), [this](const std::string& l) { this->commitAndRefresh(l); });
    m_canvas->setPosition({250.f, 60.f});
    this->addChild(m_canvas);

    auto* canvasOps = CCMenu::create();
    this->addChild(canvasOps, 5);
    float cx = 250.f;
    float cy = win.height - 150.f;
    auto addCanvas = [&](const char* label, std::function<void()> cb, float w = 54.f) {
        auto* b = makeButton(label, std::move(cb), w, 0.38f);
        b->setPosition({cx, cy});
        canvasOps->addChild(b);
        cx += w + 4.f;
    };
    addCanvas("Z+", [this]() { m_canvas->setZoom(m_canvas->zoom() + 0.25f); }, 44.f);
    addCanvas("Z-", [this]() { m_canvas->setZoom(m_canvas->zoom() - 0.25f); }, 44.f);
    addCanvas("Snap", [this]() { m_canvas->toggleSnap(); notify(m_canvas->snap() ? "Snap on" : "Snap off"); }, 54.f);
    addCanvas("L", [this]() { m_canvas->alignSelected(0); }, 34.f);
    addCanvas("R", [this]() { m_canvas->alignSelected(1); }, 34.f);
    addCanvas("T", [this]() { m_canvas->alignSelected(2); }, 34.f);
    addCanvas("B", [this]() { m_canvas->alignSelected(3); }, 34.f);
    addCanvas("H", [this]() { m_canvas->alignSelected(4); }, 34.f);
    addCanvas("V", [this]() { m_canvas->alignSelected(5); }, 34.f);
    addCanvas("Preview Popup", [this]() { this->onPreviewPopup(); }, 100.f);

    // --- Right: properties -------------------------------------------------
    m_props = PropertiesPanel::create(m_state.get(), [this](const std::string& l) { this->commitAndRefresh(l); });
    m_props->setPosition({win.width - 310.f, 60.f});
    this->addChild(m_props);

    // --- Status bar --------------------------------------------------------
    m_status = makeLabel("", "bigFont.fnt", 0.4f);
    m_status->setAnchorPoint({0.f, 0.5f});
    m_status->setPosition({10.f, 20.f});
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
