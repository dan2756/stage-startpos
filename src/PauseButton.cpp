#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

// A button under the pause menu's settings gear that opens this mod's settings
class $modify(TargetPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = this->getChildByID("right-button-menu");
        if (!menu) return;

        // The same green circle as the gear, with the mod's own icon on it
        auto sprite = CircleButtonSprite::createWithSprite(
            "blitz-icon.png"_spr, 1.f, CircleBaseColor::Green, CircleBaseSize::Medium
        );

        auto gear = menu->getChildByID("options-button");
        if (gear) sprite->setScale(gear->getContentWidth() / sprite->getContentWidth());

        auto button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(TargetPauseLayer::onTargetSettings)
        );
        button->setID("settings-button"_spr);
        menu->addChild(button);

        // The menu's column layout puts it under the gear; without one it goes there by hand
        if (menu->getLayout()) {
            menu->updateLayout();
        } else if (gear) {
            button->setPosition(gear->getPosition() - CCPoint { 0.f, gear->getContentHeight() + 5.f });
        }
    }

    void onTargetSettings(CCObject*) {
        openSettingsPopup(Mod::get());
    }
};
