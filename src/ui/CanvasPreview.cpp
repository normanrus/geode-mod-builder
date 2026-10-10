#include "ui/CanvasPreview.hpp"

#include "ui/Ui.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace gmb {

bool CanvasPreview::init(EditorState* state, std::function<void(const std::string&)> commit) {
    if (!CCLayer::init()) return false;
    m_state = state;
    m_commit = std::move(commit);
    this->setContentSize({400.f, 300.f});

    auto* bg = makePanel({400.f, 300.f}, {20, 20, 24}, 220);
    if (bg) this->addChild(bg);

    m_world = cocos2d::CCNode::create();
    m_world->setPosition({0.f, 0.f});
    this->addChild(m_world);

    m_grid = cocos2d::CCDrawNode::create();
    this->addChild(m_grid);
    this->drawGrid({400.f, 300.f});

    this->setTouchEnabled(true);
    this->setTouchMode(cocos2d::kCCTouchesOneByOne);
    this->rebuild();
    return true;
}

CanvasPreview* CanvasPreview::create(EditorState* state, std::function<void(const std::string&)> commit) {
    auto* ret = new CanvasPreview();
    if (ret->init(state, std::move(commit))) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

void CanvasPreview::drawGrid(cocos2d::CCSize const& size) {
    if (!m_grid) return;
    m_grid->clear();
    // Light dotted grid so users can align elements; snapping is opt-in.
    for (float x = 0; x <= size.width; x += m_gridStep * 2) {
        m_grid->drawDot({x, size.height / 2.f}, 0.5f, {255, 255, 255, 25});
    }
    for (float y = 0; y <= size.height; y += m_gridStep * 2) {
        m_grid->drawDot({size.width / 2.f, y}, 0.5f, {255, 255, 255, 25});
    }
}

void CanvasPreview::setZoom(float z) {
    m_zoom = std::max(0.25f, std::min(3.f, z));
    if (m_world) m_world->setScale(m_zoom);
    this->rebuild();
}

void CanvasPreview::drawElement(const Element& e, cocos2d::CCNode* parent, cocos2d::CCPoint origin,
                                float parentW, float parentH) {
    // Map editor coordinates (origin bottom-left of the container) into world
    // space. Preview only; does not use any live GD nodes.
    cocos2d::CCPoint world = {origin.x + e.x, origin.y + e.y};

    float w = e.width > 0.f ? e.width : 40.f;
    float h = e.height > 0.f ? e.height : 24.f;

    cocos2d::CCNode* node = nullptr;
    switch (e.type) {
        case ElementType::Label: {
            auto* lbl = makeLabel(e.props["text"].asString(e.name), "bigFont.fnt", 0.4f);
            node = lbl;
            break;
        }
        case ElementType::Button: {
            auto* lbl = makeLabel(e.props["text"].asString(e.name.empty() ? "Button" : e.name), "bigFont.fnt", 0.4f);
            node = lbl;
            break;
        }
        case ElementType::Sprite: {
            auto* spr = (cocos2d::CCSprite*)nullptr;
            // Try to preview the real assigned texture (read-only, preview only).
            std::string asset = e.props["asset"].asString();
            if (auto* a = m_state->project.findAsset(asset)) {
                if (!a->sourcePath.empty()) spr = cocos2d::CCSprite::create(a->sourcePath.c_str());
            }
            if (!spr) spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_button_01.png");
            if (spr) spr->setScale(0.6f);
            node = spr;
            break;
        }
        case ElementType::Checkbox: {
            auto* spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
            if (!spr) spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_button_01.png");
            node = spr;
            break;
        }
        case ElementType::Slider: {
            auto* spr = cocos2d::CCSprite::createWithSpriteFrameName("slidergroove.png");
            if (!spr) spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_button_01.png");
            if (spr) spr->setScaleX(0.5f);
            node = spr;
            break;
        }
        case ElementType::TextInput: {
            auto* spr = makePanel({w, std::max(20.f, h)}, {60, 60, 70}, 230);
            if (spr) {
                auto* ph = makeLabel(e.props["placeholder"].asString("Type here..."), "bigFont.fnt", 0.35f);
                if (ph) { ph->setPosition({0.f, 0.f}); spr->addChild(ph); }
            }
            node = spr;
            break;
        }
        case ElementType::Popup: {
            auto* spr = makePanel({std::max(60.f, w), std::max(40.f, h)}, {40, 40, 60}, 150);
            node = spr;
            break;
        }
        case ElementType::Panel: {
            auto* spr = makePanel({std::max(20.f, w), std::max(20.f, h)}, {40, 40, 48}, 180);
            node = spr;
            break;
        }
        case ElementType::CloseButton: {
            auto* spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
            node = spr;
            break;
        }
        case ElementType::Screen:
            break;
    }

    if (!node) node = cocos2d::CCNode::create();
    node->setPosition(world);
    if (e.scale != 1.f) node->setScale(node->getScale() * e.scale);
    if (e.rotation != 0.f) node->setRotation(e.rotation);
    if (e.opacity < 1.f && node->getChildrenCount() == 0)
        if (auto* rgba = typeinfo_cast<cocos2d::CCNodeRGBA*>(node))
            rgba->setOpacity(static_cast<GLubyte>(255 * e.opacity));
    if (!e.visible) {
        if (auto* rgba = typeinfo_cast<cocos2d::CCNodeRGBA*>(node))
            rgba->setOpacity(80);
    }
    if (e.id == m_state->selectedId) {
        if (auto* rgba = typeinfo_cast<cocos2d::CCNodeRGBA*>(node)) rgba->setColor({255, 220, 120});
    }
    parent->addChild(node, e.zOrder);

    // Container outline.
    if (isContainerType(e.type) && e.type != ElementType::Popup) {
        auto* outline = cocos2d::CCDrawNode::create();
        cocos2d::CCPoint verts[4] = {
            {0.f, 0.f}, {std::max(20.f, w), 0.f}, {std::max(20.f, w), std::max(20.f, h)}, {0.f, std::max(20.f, h)}};
        outline->drawPolygon(verts, 4, {0, 0, 0, 40}, 1.f, {90, 90, 110, 160});
        outline->setPosition(world);
        parent->addChild(outline, -1);
    }
}

void CanvasPreview::rebuild() {
    if (!m_world) return;
    m_world->removeAllChildren();

    const Project& p = m_state->project;

    // Render only the first screen's tree (the active screen). Screen roots are
    // placed at the canvas origin.
    if (p.screens.empty()) return;
    const Screen& s = p.screens.front();

    // Background panel to make the preview boundary explicit.
    auto* bounds = cocos2d::CCDrawNode::create();
    cocos2d::CCPoint verts[4] = {{0, 0}, {480.f, 0}, {480.f, 320.f}, {0, 320.f}};
    bounds->drawPolygon(verts, 4, {0, 0, 0, 0}, 1.f, {120, 120, 150, 200});
    m_world->addChild(bounds, -2);

    std::function<void(const std::string&, cocos2d::CCPoint, float, float)> render =
        [&](const std::string& id, cocos2d::CCPoint origin, float pw, float ph) {
            auto* e = p.findElement(id);
            if (!e) return;
            if (e->type == ElementType::Popup) return; // popups are previewed separately
            this->drawElement(*e, m_world, origin, pw, ph);

            // Children are positioned relative to their container's origin.
            cocos2d::CCPoint childOrigin = origin;
            float cw = pw, ch = ph;
            if (isContainerType(e->type)) {
                childOrigin = cocos2d::CCPoint{origin.x + e->x, origin.y + e->y};
                cw = e->width > 0.f ? e->width : pw;
                ch = e->height > 0.f ? e->height : ph;
            }
            for (const auto& c : e->children) render(c, childOrigin, cw, ch);
        };

    for (const auto& root : s.roots) render(root, {0.f, 0.f}, 480.f, 320.f);
}

std::string CanvasPreview::elementAt(cocos2d::CCPoint worldPoint, std::vector<std::string>& hits) const {
    const Project& p = m_state->project;
    std::string best;
    float bestArea = 1e18f;

    std::function<void(const std::string&, cocos2d::CCPoint, float, float)> hit =
        [&](const std::string& id, cocos2d::CCPoint origin, float pw, float ph) {
            auto* e = p.findElement(id);
            if (!e || e->type == ElementType::Popup) return;
            float w = e->width > 0.f ? e->width : 40.f;
            float h = e->height > 0.f ? e->height : 24.f;
            float cx = origin.x + e->x;
            float cy = origin.y + e->y;
            cocos2d::CCRect rect(cx - w / 2.f, cy - h / 2.f, w, h);
            if (rect.containsPoint(worldPoint)) {
                hits.push_back(id);
                float area = w * h;
                if (area < bestArea) { bestArea = area; best = id; }
            }
            cocos2d::CCPoint childOrigin = origin;
            float cw = pw, ch = ph;
            if (isContainerType(e->type)) {
                childOrigin = cocos2d::CCPoint{cx, cy};
                cw = w; ch = h;
            }
            for (const auto& c : e->children) hit(c, childOrigin, cw, ch);
        };

    for (const auto& s : p.screens)
        for (const auto& root : s.roots) hit(root, {0.f, 0.f}, 480.f, 320.f);
    return best;
}

bool CanvasPreview::ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent*) {
    auto local = this->convertToNodeSpace(touch->getLocation());
    cocos2d::CCPoint world = {local.x / m_zoom, local.y / m_zoom};

    std::vector<std::string> hits;
    std::string picked = elementAt(world, hits);

    if (picked.empty()) {
        m_state->selectedId.clear();
    } else if (picked == m_state->selectedId && hits.size() > 1) {
        // Cycle through overlapping elements (the tree also allows direct pick).
        auto it = std::find(hits.begin(), hits.end(), picked);
        size_t idx = it == hits.end() ? 0 : (it - hits.begin() + 1) % hits.size();
        m_state->selectedId = hits[idx];
    } else {
        m_state->selectedId = picked;
    }
    if (m_commit) m_commit("Select");

    if (!m_state->selectedId.empty()) {
        auto* e = m_state->project.findElement(m_state->selectedId);
        if (e) {
            m_dragging = true;
            m_dragStart = world;
            m_origPos = cocos2d::CCPoint{e->x, e->y};
        }
    }
    return true;
}

void CanvasPreview::ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent*) {
    if (!m_dragging) return;
    auto* e = m_state->project.findElement(m_state->selectedId);
    if (!e) return;
    auto local = this->convertToNodeSpace(touch->getLocation());
    cocos2d::CCPoint world = {local.x / m_zoom, local.y / m_zoom};
    float nx = m_origPos.x + (world.x - m_dragStart.x);
    float ny = m_origPos.y + (world.y - m_dragStart.y);
    if (m_snap) {
        nx = std::round(nx / m_gridStep) * m_gridStep;
        ny = std::round(ny / m_gridStep) * m_gridStep;
    }
    e->x = nx;
    e->y = ny;
    this->rebuild();
}

void CanvasPreview::ccTouchEnded(cocos2d::CCTouch*, cocos2d::CCEvent*) {
    if (m_dragging && m_commit) m_commit("Move element");
    m_dragging = false;
}

void CanvasPreview::ccTouchCancelled(cocos2d::CCTouch*, cocos2d::CCEvent*) {
    m_dragging = false;
}

void CanvasPreview::alignSelected(int mode) {
    auto* e = m_state->project.findElement(m_state->selectedId);
    if (!e) return;
    float w = e->width > 0.f ? e->width : 40.f;
    float h = e->height > 0.f ? e->height : 24.f;

    // Align within the parent container (or the screen).
    float cw = 480.f, ch = 320.f;
    cocos2d::CCPoint origin = {0.f, 0.f};
    if (!e->parent.empty()) {
        if (auto* par = m_state->project.findElement(e->parent)) {
            cw = par->width > 0.f ? par->width : cw;
            ch = par->height > 0.f ? par->height : ch;
            origin = cocos2d::CCPoint{par->x + par->width / 2.f, par->y + par->height / 2.f};
        }
    }
    switch (mode) {
        case 0: e->x = -cw / 2.f + w / 2.f; break; // left
        case 1: e->x = cw / 2.f - w / 2.f; break;  // right
        case 2: e->y = ch / 2.f - h / 2.f; break;  // top
        case 3: e->y = -ch / 2.f + h / 2.f; break; // bottom
        case 4: e->x = 0.f; break;                 // horizontal center
        case 5: e->y = 0.f; break;                 // vertical center
        default: break;
    }
    if (m_commit) m_commit("Align element");
    this->rebuild();
}

} // namespace gmb
