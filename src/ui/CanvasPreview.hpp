// Canvas preview: an editor-side rendering of the layout. This is explicitly a
// preview, separate from the real Geometry Dash interface.
//
// Supports: select (including cycling through overlapping elements), drag to
// move, numeric position editing (in the properties panel), resize handles are
// represented by width/height properties, zoom, alignment, and grid snapping.
#pragma once

#include "ui/EditorState.hpp"

#include <Geode/Geode.hpp>
#include <functional>
#include <vector>

namespace gmb {

class CanvasPreview : public cocos2d::CCLayer {
protected:
    EditorState* m_state = nullptr;
    std::function<void(const std::string&)> m_commit;
    cocos2d::CCNode* m_world = nullptr;
    cocos2d::CCDrawNode* m_grid = nullptr;

    float m_zoom = 1.f;
    bool m_snap = false;
    float m_gridStep = 10.f;

    bool m_dragging = false;
    cocos2d::CCPoint m_dragStart{};
    cocos2d::CCPoint m_origPos{};
    int m_cycleIndex = 0;

    bool init(EditorState* state, std::function<void(const std::string&)> commit);
    void rebuild();
    void drawElement(const Element& e, cocos2d::CCNode* parent, cocos2d::CCPoint origin, float parentW, float parentH);
    void drawGrid(cocos2d::CCSize const& size);
    std::string elementAt(cocos2d::CCPoint worldPoint, std::vector<std::string>& hits) const;

public:
    static CanvasPreview* create(EditorState* state, std::function<void(const std::string&)> commit);
    void refresh() { this->rebuild(); }
    void setZoom(float z);
    float zoom() const { return m_zoom; }
    void toggleSnap() { m_snap = !m_snap; }
    bool snap() const { return m_snap; }
    void alignSelected(int mode); // 0 left,1 right,2 top,3 bottom,4 hcenter,5 vcenter

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    bool isDragging() const { return m_dragging; }
};

} // namespace gmb
