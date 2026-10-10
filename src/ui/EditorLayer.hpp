// The project editor: toolbar, element tree, canvas, properties, actions and a
// status/validation area. Owns a temporary UI over a shared EditorState; the
// model itself is never stored in UI nodes.
#pragma once

#include "ui/CanvasPreview.hpp"
#include "ui/EditorState.hpp"
#include "ui/ElementTree.hpp"
#include "ui/PropertiesPanel.hpp"

#include <Geode/Geode.hpp>

namespace gmb {

class EditorLayer : public cocos2d::CCLayer {
protected:
    std::shared_ptr<EditorState> m_state;
    ElementTree* m_tree = nullptr;
    CanvasPreview* m_canvas = nullptr;
    PropertiesPanel* m_props = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelBMFont* m_title = nullptr;

    bool init() override;
    void rebuildAll();
    void updateTitleAndStatus();
    void onBack();
    void onSave();
    void onSaveAs();
    void onUndo();
    void onRedo();
    void onGenerate();
    void onBuild();
    void onActions();
    void onPreviewPopup();
    void onNewElement();
    void onNewElementType(ElementType type);

    void commitAndRefresh(const std::string& label);

public:
    static EditorLayer* create(std::shared_ptr<EditorState> state);
    static cocos2d::CCScene* scene(std::shared_ptr<EditorState> state);

    void keyBackClicked() override;
    void onExit() override;
};

} // namespace gmb
