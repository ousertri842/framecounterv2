#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/NineSlice.hpp>

// The modern dark look shared by every Frame Window Lab screen.
namespace fwl::style {

inline constexpr cocos2d::ccColor3B kPanel{20, 27, 44};
inline constexpr cocos2d::ccColor3B kPanelRaised{33, 45, 70};
inline constexpr cocos2d::ccColor3B kPanelHover{44, 60, 94};
inline constexpr cocos2d::ccColor3B kAccent{136, 180, 255};
inline constexpr cocos2d::ccColor3B kText{232, 238, 250};
inline constexpr cocos2d::ccColor3B kMuted{140, 154, 182};
inline constexpr cocos2d::ccColor3B kRecord{255, 88, 108};
inline constexpr cocos2d::ccColor3B kGood{120, 226, 150};

inline constexpr char const* kRoundedSquare = "square02b_001.png";
inline constexpr char const* kTitleFont = "bigFont.fnt";
inline constexpr char const* kBodyFont = "chatFont.fnt";

inline geode::NineSlice* panel(cocos2d::CCSize size, cocos2d::ccColor3B color, GLubyte opacity = 255) {
    auto bg = geode::NineSlice::create(kRoundedSquare);
    bg->setContentSize(size);
    bg->setColor(color);
    bg->setOpacity(opacity);
    bg->setScaleMultiplier(0.5f);
    return bg;
}

inline cocos2d::CCLabelBMFont* label(
    std::string const& text, char const* font, float scale, cocos2d::ccColor3B color = kText
) {
    auto lbl = cocos2d::CCLabelBMFont::create(text.c_str(), font);
    lbl->setScale(scale);
    lbl->setColor(color);
    return lbl;
}

// A flat rounded button face: tinted background with a centred caption.
inline cocos2d::CCNode* buttonFace(
    std::string const& text, cocos2d::ccColor3B textColor, float width, float height = 26.f,
    cocos2d::ccColor3B bgColor = kPanelRaised
) {
    auto node = cocos2d::CCNode::create();
    node->setContentSize({width, height});
    node->setAnchorPoint({0.5f, 0.5f});

    auto bg = panel({width, height}, bgColor);
    bg->setPosition({width / 2, height / 2});
    node->addChild(bg);

    auto lbl = label(text, kTitleFont, 0.42f, textColor);
    lbl->limitLabelWidth(width - 12.f, 0.42f, 0.2f);
    lbl->setPosition({width / 2, height / 2 + 1.f});
    node->addChild(lbl);
    return node;
}

} // namespace fwl::style
