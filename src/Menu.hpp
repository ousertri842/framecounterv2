#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

namespace fwl {

// The Frame Window Lab menu, opened from the pause screen.
class LabPopup : public geode::Popup {
public:
    static LabPopup* create();

protected:
    bool init();
    void rebuild();
    void runAndClose(geode::Result<> (*action)());
    void confirmDelete();

    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCMenu* m_actions = nullptr;
};

// The round button added to the pause screen.
cocos2d::CCNode* createPauseButtonSprite();

} // namespace fwl
