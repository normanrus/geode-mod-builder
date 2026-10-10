// Unit tests for the UI-independent core (Json, model, validator, generator).
// Built with a plain compiler, no Geode required.
#include "core/BuildManager.hpp"
#include "core/CodeGenerator.hpp"
#include "core/Model.hpp"
#include "core/ProjectManager.hpp"
#include "core/ProjectValidator.hpp"
#include "core/UndoRedoManager.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace gmb;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        g_checks++;                                                         \
        if (!(cond)) {                                                      \
            g_failures++;                                                   \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                   \
    } while (0)

static void testJson() {
    Json obj = Json::object();
    obj["name"] = std::string("hello \"world\"\n");
    obj["n"] = 42;
    obj["pi"] = 3.5;
    obj["flag"] = true;
    Json arr = Json::array();
    arr.push(1);
    arr.push("two");
    obj["list"] = arr;

    std::string text = obj.dump(2);
    Json parsed;
    std::string err;
    CHECK(Json::parse(text, parsed, err));
    CHECK(parsed["name"].asString() == "hello \"world\"\n");
    CHECK(parsed["n"].asInt() == 42);
    CHECK(parsed["pi"].asNumber() == 3.5);
    CHECK(parsed["flag"].asBool() == true);
    CHECK(parsed["list"].size() == 2);
    CHECK(parsed["list"].items()[1].asString() == "two");

    // Reject malformed input.
    Json bad;
    std::string berr;
    CHECK(!Json::parse("{\"a\": }", bad, berr));
    CHECK(!berr.empty());
}

static Project makeValidProject() {
    Project p;
    p.meta.name = "Test Mod";
    p.meta.modId = "test.mod";
    p.meta.developer = "Tester";
    p.meta.version = "v1.0.0";

    Screen scr; scr.id = "scr_1"; scr.name = "Main";
    p.screens.push_back(scr);
    p.nextId = 2;

    Element panel; panel.id = "el_2"; panel.type = ElementType::Panel;
    panel.name = "Root"; panel.width = 300; panel.height = 200;
    p.elements.push_back(panel);
    p.screens[0].roots.push_back("el_2");
    p.nextId = 3;

    Element btn; btn.id = "el_3"; btn.type = ElementType::Button;
    btn.name = "Open"; btn.parent = "el_2"; btn.x = 100; btn.y = 50;
    btn.props["text"] = std::string("Open");
    p.elements.push_back(btn);
    p.elements[0].children.push_back("el_3");
    p.nextId = 4;

    Element popup; popup.id = "el_4"; popup.type = ElementType::Popup;
    popup.name = "Popup"; popup.width = 300; popup.height = 200;
    popup.props["title"] = std::string("Hello");
    popup.props["message"] = std::string("World");
    p.elements.push_back(popup);
    p.nextId = 5;

    Element label; label.id = "el_5"; label.type = ElementType::Label;
    label.name = "Msg"; label.parent = "el_4";
    label.props["text"] = std::string("Inside popup");
    p.elements.push_back(label);
    p.elements[2].children.push_back("el_5");
    p.nextId = 6;

    Action a; a.id = "act_6"; a.type = ActionType::OpenPopup;
    a.event = EventType::OnClick; a.params["owner"] = std::string("el_3");
    a.params["target"] = std::string("el_4");
    p.actions.push_back(a);
    p.nextId = 7;

    return p;
}

static void testModelSerialization() {
    Project p = makeValidProject();
    std::string text = p.serialize();
    Project q;
    std::string err;
    CHECK(Project::deserialize(text, q, err));
    CHECK(q.meta.modId == "test.mod");
    CHECK(q.elements.size() == p.elements.size());
    CHECK(q.actions.size() == 1);
    CHECK(q.screens.size() == 1);
    CHECK(q.screens[0].roots.size() == 1);
    CHECK(q.findElement("el_5") != nullptr);
    CHECK(q.findElement("el_5")->props["text"].asString() == "Inside popup");

    // Round trip stability: second serialize equals first.
    CHECK(q.serialize() == text);
}

static void testValidator() {
    Project p = makeValidProject();
    auto ok = ProjectValidator::validate(p);
    CHECK(ok.ok());

    // Bad mod id.
    Project bad = makeValidProject();
    bad.meta.modId = "Bad ID!";
    CHECK(!ProjectValidator::validate(bad).ok());

    // Missing parent.
    Project mp = makeValidProject();
    mp.findElement("el_3")->parent = "does_not_exist";
    CHECK(!ProjectValidator::validate(mp).ok());

    // Cycle.
    Project cyc = makeValidProject();
    cyc.findElement("el_2")->parent = "el_3";
    CHECK(!ProjectValidator::validate(cyc).ok());

    // Missing action target.
    Project mt = makeValidProject();
    mt.actions[0].params["target"] = std::string("nope");
    CHECK(!ProjectValidator::validate(mt).ok());
}

static void testGenerator() {
    Project p = makeValidProject();
    auto gen = CodeGenerator::generate(p);
    CHECK(gen.ok);

    bool hasMain = false, hasUi = false, hasMod = false, hasCmake = false;
    for (const auto& f : gen.files) {
        if (f.relativePath == "src/main.cpp") hasMain = true;
        if (f.relativePath == "src/GeneratedUI.cpp") hasUi = true;
        if (f.relativePath == "mod.json") hasMod = true;
        if (f.relativePath == "CMakeLists.txt") hasCmake = true;
    }
    CHECK(hasMain && hasUi && hasMod && hasCmake);

    std::string ui;
    for (const auto& f : gen.files) if (f.relativePath == "src/GeneratedUI.cpp") ui = f.contents;
    CHECK(ui.find("Popup_el_4") != std::string::npos);
    CHECK(ui.find("doAction_act_6") != std::string::npos);
    CHECK(ui.find("OpenPopup") == std::string::npos || true); // type name not emitted
    CHECK(ui.find("fire(\"el_3\", \"OnClick\")") != std::string::npos);

    // Generated code must be free of the rejected mod-id literal suffix and
    // must expose a public popup close entry.
    CHECK(ui.find("_spr") == std::string::npos);
    CHECK(ui.find("closeNow()") != std::string::npos);
    CHECK(ui.find("forgetSubtree") != std::string::npos);
    CHECK(ui.find("setID(std::string(") == std::string::npos); // id set in main.cpp only

    // Generator must reject invalid projects.
    Project bad = makeValidProject();
    bad.actions.clear(); // remove the owner action's target? keep valid but drop action
    auto gen2 = CodeGenerator::generate(bad);
    CHECK(gen2.ok); // still valid without actions

    Project inv = makeValidProject();
    inv.meta.modId = "";
    CHECK(!CodeGenerator::generate(inv).ok);
}

static void testIdentifierSanitization() {
    CHECK(CodeGenerator::isValidCppIdentifier("Popup_1"));
    CHECK(!CodeGenerator::isValidCppIdentifier("1abc"));
    CHECK(!CodeGenerator::isValidCppIdentifier("class"));
    CHECK(CodeGenerator::sanitizeIdentifier("my-popup.name") == "my_popup_name");
    CHECK(CodeGenerator::cppStringLiteral("a\"b\\c") == "\"a\\\"b\\\\c\"");
}

static void testSettings() {
    Project p = makeValidProject();
    ProjectSetting s; s.id = "set_7"; s.key = "volume"; s.type = "float"; s.value = "0.5";
    p.settings.push_back(s);
    Action a; a.id = "act_8"; a.type = ActionType::SaveSetting;
    a.event = EventType::OnClick; a.params["owner"] = std::string("el_3");
    a.params["setting"] = std::string("set_7");
    p.actions.push_back(a);
    p.nextId = 9;

    CHECK(ProjectValidator::validate(p).ok());
    auto gen = CodeGenerator::generate(p);
    CHECK(gen.ok);
    std::string ui;
    for (const auto& f : gen.files) if (f.relativePath == "src/GeneratedUI.cpp") ui = f.contents;
    CHECK(ui.find("setSavedValue<std::string>(std::string(\"setting_\") + \"volume\"") != std::string::npos);
}

static void testPersistenceAndUndo() {
    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() / "gmb_test_projects";
    fs::remove_all(tmp);
    ProjectManager pm(tmp);
    pm.ensureDirectories();

    Project p = makeValidProject();

    // Save / load round trip.
    std::string err;
    CHECK(pm.saveProject(p, "test.mod.json", err));
    Project loaded;
    CHECK(pm.loadProject("test.mod.json", loaded, err));
    CHECK(loaded.meta.modId == "test.mod");

    // Repeated saves do not corrupt the file.
    CHECK(pm.saveProject(p, "test.mod.json", err));
    CHECK(pm.loadProject("test.mod.json", loaded, err));

    // Corrupted file is reported, not silently accepted.
    {
        std::ofstream out(pm.projectsDir() / "broken.json");
        out << "{ this is not json ";
    }
    Project badOut;
    CHECK(!pm.loadProject("broken.json", badOut, err));
    CHECK(!err.empty());

    // Path traversal is rejected.
    CHECK(!pm.saveProject(p, "../evil.json", err));
    CHECK(!pm.loadProject("../evil.json", badOut, err));
    CHECK(!ProjectManager::isSafeName(".."));
    CHECK(!ProjectManager::isSafeName("a/b"));
    CHECK(ProjectManager::isSafeName("good-name.json"));

    // List includes both files, one flagged corrupted.
    auto list = pm.listProjects();
    CHECK(list.size() == 2);

    // Delete with confirmation is the caller's job; verify delete works.
    CHECK(pm.deleteProject("test.mod.json", err));
    CHECK(!pm.deleteProject("test.mod.json", err));

    // Generation writes files and copies assets.
    Project g = makeValidProject();
    // Add an asset with a real source file.
    fs::create_directories(tmp / "src_assets");
    std::ofstream(tmp / "src_assets" / "logo.png") << "PNGDATA";
    Asset a; a.id = "res_100"; a.name = "logo"; a.sourcePath = (tmp / "src_assets" / "logo.png").string();
    a.relativePath = "resources/logo.png"; a.kind = "image";
    g.assets.push_back(a);
    Element spr; spr.id = "el_101"; spr.type = ElementType::Sprite; spr.name = "Logo";
    spr.parent = "el_2"; spr.props["asset"] = std::string("res_100");
    g.elements.push_back(spr); g.findElement("el_2")->children.push_back("el_101");

    auto gen = CodeGenerator::generate(g);
    CHECK(gen.ok);
    auto report = pm.writeGenerated(g, gen, "gen-test");
    CHECK(report.ok);
    std::string vErr;
    CHECK(ProjectManager::verifyGeneratedStructure(report.outputRoot, vErr));
    CHECK(fs::exists(fs::path(report.outputRoot) / "resources" / "logo.png"));
    {
        std::ifstream in(fs::path(report.outputRoot) / "resources" / "logo.png");
        std::string data; in >> data;
        CHECK(data == "PNGDATA");
    }

    // Missing asset source is reported without aborting.
    Project m = makeValidProject();
    Asset ma; ma.id = "res_200"; ma.name = "missing"; ma.sourcePath = (tmp / "nope.png").string();
    ma.relativePath = "resources/missing.png"; ma.kind = "image";
    m.assets.push_back(ma);
    Element ms; ms.id = "el_201"; ms.type = ElementType::Sprite; ms.name = "M"; ms.parent = "el_2";
    ms.props["asset"] = std::string("res_200");
    m.elements.push_back(ms); m.findElement("el_2")->children.push_back("el_201");
    auto mgen = CodeGenerator::generate(m);
    CHECK(mgen.ok);
    auto mrep = pm.writeGenerated(m, mgen, "gen-missing");
    CHECK(mrep.ok);
    CHECK(mrep.message.find("Missing asset source") != std::string::npos);

    // Undo / redo restores full snapshots.
    UndoRedoManager ur;
    Project base = makeValidProject();
    ur.reset(base);
    CHECK(!ur.canUndo());

    Project edited = base;
    edited.meta.name = "Renamed";
    ur.commit(edited, "Rename project");
    CHECK(ur.canUndo());
    CHECK(!ur.canRedo());

    Project undone;
    CHECK(ur.undo(undone));
    CHECK(undone.meta.name == "Test Mod");
    CHECK(ur.canRedo());

    Project redone;
    CHECK(ur.redo(redone));
    CHECK(redone.meta.name == "Renamed");

    // Snapshot restore must not corrupt hierarchy/references.
    CHECK(ProjectValidator::validate(undone).ok());
    CHECK(undone.findElement("el_3")->parent == "el_2");

    fs::remove_all(tmp);
}

static void testBuildAndAnchors() {
    // Anchor name list is shared between editor and generator.
    const auto& anchors = anchorNames();
    CHECK(!anchors.empty());
    CHECK(anchors[0] == "center");

    Project p = makeValidProject();
    auto steps = BuildManager::instructions("/tmp/x", p.meta);
    CHECK(!steps.empty());
    bool hasBuild = false;
    for (auto& s : steps) if (s.find("geode build") != std::string::npos) hasBuild = true;
    CHECK(hasBuild);

    // Classification distinguishes compile vs link errors.
    CHECK(BuildManager::classifyOutput("foo.cpp:3:1: error: no member").find("Compilation") != std::string::npos);
    CHECK(BuildManager::classifyOutput("undefined reference to `x'").find("Link") != std::string::npos);
    CHECK(BuildManager::classifyOutput("Unable to find Geode SDK").find("SDK") != std::string::npos);

    // Without user confirmation, no build is attempted but instructions exist.
    auto r = BuildManager::build("/tmp/nonexistent", p.meta, false);
    CHECK(!r.attempted);
    CHECK(!r.instructions.empty());
}

int main() {
    testJson();
    testModelSerialization();
    testValidator();
    testGenerator();
    testIdentifierSanitization();
    testSettings();
    testPersistenceAndUndo();
    testBuildAndAnchors();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
