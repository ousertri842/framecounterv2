#pragma once

#include "Data.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

namespace fwl {

// Everything Frame Window Lab draws inside a level: the counter rows, the
// click circles along the path, and the recording / scanning status.
class Overlay : public cocos2d::CCNode {
public:
    static Overlay* create(PlayLayer* layer);
    static Overlay* get();
    ~Overlay() override;

    void onModeChanged();
    void resetPlayback();
    void onPlaybackInput(size_t index);
    void update(float dt) override;

private:
    bool initWithLayer(PlayLayer* layer);
    void buildCounter();
    void refreshCounter();
    void buildHud();
    void addCircle(Input const& in, cocos2d::CCPoint where, cocos2d::ccColor3B color,
        std::string const& text, double ms);

    PlayLayer* m_layer = nullptr;
    geode::Ref<cocos2d::CCNode> m_circles;

    cocos2d::CCNode* m_counter = nullptr;
    std::vector<cocos2d::CCLabelBMFont*> m_countLabels;
    std::vector<int> m_counts;

    cocos2d::CCNode* m_pill = nullptr;
    cocos2d::CCLabelBMFont* m_pillLabel = nullptr;
    cocos2d::CCNode* m_pillDot = nullptr;

    cocos2d::CCLayerColor* m_dim = nullptr;
    cocos2d::CCNode* m_scanPanel = nullptr;
    cocos2d::CCLabelBMFont* m_scanTitle = nullptr;
    cocos2d::CCLabelBMFont* m_scanStatus = nullptr;
    cocos2d::CCLayerColor* m_scanFill = nullptr;
    float m_barWidth = 0.f;
    float m_blink = 0.f;
};

} // namespace fwl
