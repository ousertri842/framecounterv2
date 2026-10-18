// Frame Window Lab: record a run, measure every click's frame window, and
// play it back with circles, counters and sounds.

#include "Lab.hpp"
#include "Menu.hpp"
#include "Overlay.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;
using namespace fwl;

class $modify(FWLBaseLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        // Run before other mods' input hooks, so a blocked input stays blocked.
        (void)self.setHookPriority("GJBaseGameLayer::handleButton", Priority::Early);
    }

    void update(float dt) {
        auto& lab = Lab::get();
        if (lab.attached(this) && !lab.manualStep() && lab.onFrame(dt)) return;
        GJBaseGameLayer::update(dt);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& lab = Lab::get();
        if (!lab.attached(this)) return GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        lab.beforeStep(this);
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        lab.afterStep(this);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto& lab = Lab::get();
        if (lab.attached(this) && !lab.injecting()) {
            // Your own clicks don't reach the game during playback and scans.
            if (lab.mode() == Mode::Playing || lab.mode() == Mode::Scanning) return;
            if (lab.mode() == Mode::Recording) lab.onRealInput(this, down, button, isPlayer1);
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

class $modify(FWLPlayLayer, PlayLayer) {
    struct Fields {
        PlayLayer* owner = nullptr;
        // Runs when the level is destroyed, however it was left.
        ~Fields() {
            if (owner) Lab::get().detach(owner);
        }
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        m_fields->owner = this;
        Lab::get().attach(this);
        if (auto overlay = Overlay::create(this)) this->addChild(overlay, 1000);
        return true;
    }

    void onQuit() {
        Lab::get().detach(this);
        PlayLayer::onQuit();
    }

    void resetLevel() {
        auto& lab = Lab::get();
        bool ours = lab.attached(this);
        if (ours) lab.onResetBefore();
        PlayLayer::resetLevel();
        if (ours) lab.onResetAfter();
    }

    void pauseGame(bool unfocused) {
        auto& lab = Lab::get();
        if (lab.attached(this)) lab.onPause();
        PlayLayer::pauseGame(unfocused);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto& lab = Lab::get();
        if (lab.attached(this) && lab.onDeath(player, object)) return;
        PlayLayer::destroyPlayer(player, object);
    }

    void playEndAnimationToPos(CCPoint position) {
        auto& lab = Lab::get();
        if (lab.attached(this) && lab.onLevelEnd()) return;
        PlayLayer::playEndAnimationToPos(position);
    }

    void levelComplete() {
        auto& lab = Lab::get();
        if (lab.attached(this)) {
            // A scan never completes the level for real.
            if (lab.mode() == Mode::Scanning) return;
            if (lab.mode() == Mode::Recording) lab.onLevelEnd();
        }
        PlayLayer::levelComplete();
    }
};

class $modify(FWLPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        if (!PlayLayer::get()) return;

        auto item = CCMenuItemExt::createSpriteExtra(createPauseButtonSprite(), [](auto*) {
            if (auto popup = LabPopup::create()) popup->show();
        });
        item->setID("open-button"_spr);

        if (auto rightMenu = this->getChildByID("right-button-menu")) {
            rightMenu->addChild(item);
            rightMenu->updateLayout();
        }
        else {
            auto win = CCDirector::get()->getWinSize();
            auto menu = CCMenu::create();
            menu->setID("menu"_spr);
            menu->setPosition({win.width - 28.f, win.height / 2.f});
            menu->addChild(item);
            this->addChild(menu, 10);
        }
    }
};

$on_mod(Loaded) {
    log::info("Frame Window Lab loaded");
}
