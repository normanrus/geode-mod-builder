// Geode Mod Builder - mod entry point.
//
// Adds a single button to the main menu that opens the builder. Uses the
// documented MenuLayer hook and the bottom-menu node id, so no manual addresses
// and no duplicate buttons (the node id guards against double-adds).
#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "ui/BuilderLayer.hpp"

using namespace geode::prelude;

class $modify(GMBMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        // Guard against adding the button twice if init runs again.
        if (this->getChildByIDRecursive("mod-builder-button"_spr)) return true;

        auto* btn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Mod Builder"),
            [](CCMenuItemSpriteExtra*) {
                auto manager = std::make_shared<gmb::ProjectManager>(Mod::get()->getSaveDir());
                CCDirector::sharedDirector()->pushScene(gmb::BuilderLayer::scene(std::move(manager)));
            });
        if (!btn) return true;
        btn->setID("mod-builder-button"_spr);

        if (auto* menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }
};
