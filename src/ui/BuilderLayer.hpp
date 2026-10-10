// Main entry screen: lists projects, creates, opens and deletes them.
#pragma once

#include "ui/EditorState.hpp"

#include <Geode/Geode.hpp>

namespace gmb {

class BuilderLayer : public cocos2d::CCLayer {
protected:
    std::shared_ptr<ProjectManager> m_manager;
    cocos2d::CCNode* m_listContainer = nullptr;

    bool init() override;
    void onNewProject(cocos2d::CCObject*);
    void onRefresh(cocos2d::CCObject*);
    void onSettings(cocos2d::CCObject*);
    void onHelp(cocos2d::CCObject*);
    void rebuildList();
    void rebuildRecent();

public:
    static BuilderLayer* create(std::shared_ptr<ProjectManager> manager);
    static cocos2d::CCScene* scene(std::shared_ptr<ProjectManager> manager);

    void openProject(const std::string& fileName);

    void keyBackClicked() override;

protected:
    cocos2d::CCNode* m_recentContainer = nullptr;
};

} // namespace gmb
