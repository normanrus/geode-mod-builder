#include "ui/BuilderLayer.hpp"

#include "ui/CreateProjectPopup.hpp"
#include "ui/EditorLayer.hpp"
#include "ui/Ui.hpp"

#include <Geode/ui/ScrollLayer.hpp>
#include <ctime>

using namespace geode::prelude;

namespace gmb {

static std::string nowIso() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

bool BuilderLayer::init() {
    if (!CCLayer::init()) return false;
    if (!m_manager) { FLAlertLayer::create("Error", "No project manager.", "OK")->show(); return false; }
    m_manager->ensureDirectories();

    auto win = CCDirector::sharedDirector()->getWinSize();

    auto* bg = createLayerBG();
    if (bg) this->addChild(bg);

    auto* title = makeLabel("Geode Mod Builder", "goldFont.fnt", 0.8f);
    title->setPosition({win.width / 2.f, win.height - 22.f});
    this->addChild(title);

    // Top action row. Uses CCMenu + equal spacing so buttons never overlap at
    // the fixed GD design resolution.
    auto* top = CCMenu::create();
    top->setPosition({win.width / 2.f, win.height - 52.f});
    top->setScale(0.5f);
    this->addChild(top, 5);
    auto addTop = [&](const char* label, std::function<void()> cb) {
        if (auto* b = makeButton(label, std::move(cb), 0.5f)) top->addChild(b);
    };
    addTop("New Project", [this]() { this->onNewProject(nullptr); });
    addTop("Refresh",     [this]() { this->onRefresh(nullptr); });
    addTop("Settings",    [this]() { this->onSettings(nullptr); });
    addTop("Help",        [this]() { this->onHelp(nullptr); });
    top->alignItemsHorizontallyWithPadding(6.f);

    // Back to the main menu.
    auto* backMenu = CCMenu::create();
    backMenu->setPosition({0.f, 0.f});
    this->addChild(backMenu, 5);
    if (auto* back = makeButton("Back", [this]() {
            CCDirector::sharedDirector()->popScene();
        }, 0.5f)) {
        back->setPosition({40.f, 20.f});
        backMenu->addChild(back);
    }

    // Scrollable project list.
    float listW = 360.f;
    float listTop = win.height - 128.f;
    float listBottom = 46.f;
    auto* scroll = ScrollLayer::create({listW, listTop - listBottom});
    scroll->setPosition({win.width / 2.f - listW / 2.f, listBottom});
    this->addChild(scroll);
    m_listContainer = scroll->m_contentLayer;

    // Recent projects (top few, newest first).
    auto* recentLabel = makeLabel("Recent Projects", "goldFont.fnt", 0.5f);
    recentLabel->setAnchorPoint({0.5f, 0.5f});
    recentLabel->setPosition({win.width / 2.f, listTop - 4.f});
    this->addChild(recentLabel);

    auto* recent = CCNode::create();
    recent->setPosition({win.width / 2.f, listTop - 24.f});
    this->addChild(recent);
    m_recentContainer = recent;

    this->rebuildList();
    this->rebuildRecent();
    this->setKeypadEnabled(true);
    return true;
}

void BuilderLayer::rebuildRecent() {
    if (!m_recentContainer) return;
    m_recentContainer->removeAllChildren();

    auto projects = m_manager->listProjects();
    std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
        return a.modifiedAt > b.modifiedAt;
    });

    auto* menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    m_recentContainer->addChild(menu);

    if (projects.empty()) {
        auto* none = makeLabel("No recent projects.", "bigFont.fnt", 0.35f);
        none->setPosition({0.f, 0.f});
        m_recentContainer->addChild(none);
        return;
    }

    int shown = 0;
    float x = -150.f;
    for (const auto& info : projects) {
        if (shown >= 4) break;
        std::string fn = info.fileName;
        auto* b = makeButton(info.displayName, [this, fn]() { this->openProject(fn); }, 0.35f);
        b->setPosition({x, 0.f});
        menu->addChild(b);
        x += 100.f;
        shown++;
    }
}

void BuilderLayer::rebuildList() {
    if (!m_listContainer) return;
    m_listContainer->removeAllChildren();

    auto projects = m_manager->listProjects();
    std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
        return a.modifiedAt > b.modifiedAt; // most recent first
    });

    const float rowH = 34.f;
    float total = std::max(1.f, static_cast<float>(projects.size()) * (rowH + 4.f));
    float width = m_listContainer->getContentSize().width;
    m_listContainer->setContentSize({width, total});

    if (projects.empty()) {
        auto* empty = makeLabel("No projects yet. Create one to start.", "bigFont.fnt", 0.45f);
        empty->setPosition({width / 2.f, total - 20.f});
        m_listContainer->addChild(empty);
        return;
    }

    float y = total - 20.f;
    for (const auto& info : projects) {
        auto* row = CCMenu::create();
        row->setPosition({0.f, 0.f});
        m_listContainer->addChild(row);

        std::string label = info.displayName;
        if (!info.modifiedAt.empty()) label += "  (" + info.modifiedAt + ")";
        auto* nameLbl = makeLabel(label, "bigFont.fnt", 0.45f);
        nameLbl->setAnchorPoint({0.f, 0.5f});
        nameLbl->setPosition({10.f, y});
        m_listContainer->addChild(nameLbl);

        std::string fn = info.fileName;
        auto* openBtn = makeButton("Open", [this, fn]() { this->openProject(fn); });
        openBtn->setPosition({width - 130.f, y});
        row->addChild(openBtn);

        auto* delBtn = makeButton("Delete", [this, fn, name = info.displayName]() {
            std::string prompt = "Delete project \"" + name + "\"? This cannot be undone.";
            createQuickPopup("Confirm Delete", prompt.c_str(), "Cancel", "Delete",
                [this, fn](FLAlertLayer*, bool btn2) {
                    if (!btn2) return;
                    std::string err;
                    if (m_manager->deleteProject(fn, err)) {
                        notify("Project deleted.", NotificationIcon::Success);
                    } else {
                        notify("Delete failed: " + err, NotificationIcon::Error);
                    }
                    this->rebuildList();
                    this->rebuildRecent();
                });
        });
        delBtn->setPosition({width - 60.f, y});
        row->addChild(delBtn);

        y -= (rowH + 4.f);
    }
}

void BuilderLayer::onNewProject(cocos2d::CCObject*) {
    CreateProjectPopup::create([this](Project p) {
        std::string base = p.meta.name.empty() ? p.meta.modId : p.meta.name;
        std::string used;
        std::string err;
        p.meta.createdAt = nowIso();
        p.meta.modifiedAt = nowIso();
        if (!m_manager->saveProjectAs(p, base, used, err)) {
            notify("Could not save project: " + err, NotificationIcon::Error);
            return;
        }
        notify("Project created.", NotificationIcon::Success);
        this->openProject(used);
    })->show();
}

void BuilderLayer::onRefresh(cocos2d::CCObject*) {
    this->rebuildList();
    this->rebuildRecent();
}

void BuilderLayer::keyBackClicked() {
    CCDirector::sharedDirector()->popScene();
}

void BuilderLayer::onSettings(cocos2d::CCObject*) {
    auto t = ProjectManager::detectToolchain();
    std::string msg = "Builder settings and environment\n\n";
    msg += "Geode CLI: " + std::string(t.geodeCliFound ? "found" : "missing") + "\n";
    if (!t.geodeCliVersion.empty()) msg += "  " + t.geodeCliVersion + "\n";
    msg += "GEODE_SDK: " + std::string(t.sdkFound ? t.sdkPath : "not set") + "\n";
    msg += "CMake: " + std::string(t.cmakeFound ? "found" : "missing") + "\n";
    msg += "Compiler: " + std::string(t.compilerFound ? t.compilerPath : "missing") + "\n\n";
    msg += "Projects directory:\n" + m_manager->projectsDir().string() + "\n";
    msg += "Generated directory:\n" + m_manager->generatedDir().string();
    FLAlertLayer::create("Settings", msg.c_str(), "OK")->show();
}

void BuilderLayer::onHelp(cocos2d::CCObject*) {
    std::string msg =
        "Geode Mod Builder lets you build simple Geode mods visually.\n\n"
        "1. Create a project and fill in its metadata.\n"
        "2. Add elements (button, label, sprite, popup, ...).\n"
        "3. Configure properties and wire up actions.\n"
        "4. Preview the layout, then Generate Code.\n"
        "5. Build the generated project if you have the Geode SDK.\n\n"
        "The preview is an editor preview, not the live Geometry Dash UI.";
    FLAlertLayer::create("Help", msg.c_str(), "OK")->show();
}

void BuilderLayer::openProject(const std::string& fileName) {
    Project p;
    std::string err;
    if (!m_manager->loadProject(fileName, p, err)) {
        notify("Could not open project: " + err, NotificationIcon::Error);
        return;
    }
    auto state = std::make_shared<EditorState>();
    state->manager = m_manager;
    state->project = std::move(p);
    state->fileName = fileName;
    state->open = true;
    state->resetHistory();
    CCDirector::sharedDirector()->pushScene(EditorLayer::scene(state));
}

BuilderLayer* BuilderLayer::create(std::shared_ptr<ProjectManager> manager) {
    auto* ret = new BuilderLayer();
    ret->m_manager = std::move(manager);
    if (ret->init()) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

cocos2d::CCScene* BuilderLayer::scene(std::shared_ptr<ProjectManager> manager) {
    auto* scene = cocos2d::CCScene::create();
    scene->addChild(BuilderLayer::create(std::move(manager)));
    return scene;
}

} // namespace gmb
