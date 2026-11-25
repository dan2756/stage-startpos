#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include "RunTarget.hpp"

using namespace geode::prelude;

namespace {
    // Blitzkrieg has no API, so its state is read the way it stores it: the
    // level's profile link in its saved values, the stages in its save dir.
    constexpr auto BLITZKRIEG_ID = "zhulis.blitzkrieg";

    // The tolerance Blitzkrieg matches a run's start and end against a range with
    constexpr float RUN_EPS = target::RUN_EPS;

    constexpr ccColor4B MARK_COLOR = { 255, 255, 255, 255 };
    constexpr float MARK_WIDTH = 1.5f;

    // The chime from resources/chime.wav, loaded the first time it plays
    FMOD::Sound* chimeSound() {
        static bool tried = false;
        static FMOD::Sound* sound = nullptr;
        if (tried) return sound;

        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine || !engine->m_system) return nullptr;
        tried = true;

        auto path = utils::string::pathToString(Mod::get()->getResourcesDir() / "chime.wav");
        auto result = engine->m_system->createSound(
            path.c_str(), FMOD_DEFAULT | FMOD_CREATESAMPLE | FMOD_LOOP_OFF, nullptr, &sound
        );
        if (result != FMOD_OK) {
            log::warn("Couldn't load the chime {} (FMOD error {})", path, static_cast<int>(result));
            sound = nullptr;
        }
        return sound;
    }

    // At the chime volume times the game's own sound effect volume
    void playChime(float volume) {
        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine) return;

        volume *= engine->m_sfxVolume;
        if (volume <= 0.f) return;

        auto sound = chimeSound();
        if (!sound) return;

        FMOD::Channel* channel = nullptr;
        if (engine->m_system->playSound(sound, nullptr, true, &channel) != FMOD_OK || !channel) return;
        channel->setVolume(volume);
        channel->setPaused(false);
    }

    // The file of the Blitzkrieg profile linked to the level
    std::optional<std::filesystem::path> linkedProfile(GJGameLevel* level) {
        auto blitz = Loader::get()->getLoadedMod(BLITZKRIEG_ID);
        if (!blitz) return std::nullopt;

        int levelID = level->m_levelID.value();
        if (!levelID) levelID = EditorIDs::getID(level);

        auto profiles = blitz->getSaveDir() / "profiles";
        auto index = file::readFromJson<std::vector<std::string>>(profiles / "index.json");
        if (!index) return std::nullopt;

        for (auto const& id : index.unwrap()) {
            if (!blitz->getSavedValue<std::string>(fmt::format("{}-{}", levelID, id)).empty()) {
                return profiles / fmt::format("{}.json", id);
            }
        }
        return std::nullopt;
    }
}

class $modify(TargetPlayLayer, PlayLayer) {
    struct Fields {
        // Whether the level has a Blitzkrieg profile, and this attempt's run in it
        bool m_tracked = false;
        float m_start = 0.f;
        std::optional<float> m_target;
        bool m_resetting = false;
        bool m_pending = true;

        int m_decimals = 2;
        float m_slashGap = 0.f;
        bool m_percentBelow = true;
        bool m_showMarks = true;

        // The label's text without the target, and the text last written to it
        std::string m_base;
        std::string m_written;

        // Where the game had the label before it went under the bar
        bool m_stacked = false;
        CCPoint m_labelPos;
        CCPoint m_labelAnchor;

        CCLayerColor* m_startMark = nullptr;
        CCLayerColor* m_targetMark = nullptr;
        bool m_marksDirty = false;

        // What happens when the run reaches its target, and whether it has this attempt
        bool m_green = true;
        ccColor3B m_greenColor = { 85, 255, 85 };
        bool m_chime = true;
        float m_chimeVolume = 0.7f;
        bool m_chimed = false;

        // Restore upstream styling before the next update, then apply completion
        // styling last so Golden Best cannot mask a completed run.
        bool m_styled = false;
        std::string m_originalFont;
        ccColor3B m_originalColor = ccWHITE;
    };

    static void onModify(auto& self) {
        // The target goes after whatever other mods turn the percentage into
        if (!self.setHookPriorityPost("PlayLayer::updateProgressbar", Priority::VeryLate)) {
            log::warn("Failed to set hook priority.");
        }
        if (!self.setHookPriorityPost("PlayLayer::resetLevel", Priority::VeryLate)) {
            log::warn("Failed to set reset hook priority.");
        }
    }

    void resetLevel() {
        auto fields = m_fields.self();
        fields->m_resetting = true;
        fields->m_pending = true;
        fields->m_tracked = false;
        fields->m_target = std::nullopt;
        fields->m_marksDirty = true;
        fields->m_chimed = false;
        this->restoreLabel();

        PlayLayer::resetLevel();

        fields->m_start = this->getCurrentPercent();
        fields->m_resetting = false;
    }

    // Resolve on the first stable progress update, after every reset hook and
    // the start-position switcher have finished changing the selected start.
    void refreshTarget() {
        auto fields = m_fields.self();
        fields->m_pending = false;

        auto mod = Mod::get();
        if (!m_level || !mod->getSettingValue<bool>("enabled") || m_level->isPlatformer()) return;

        fields->m_decimals = mod->getSettingValue<int64_t>("decimals");
        fields->m_slashGap = mod->getSettingValue<double>("slash-gap");
        fields->m_percentBelow = mod->getSettingValue<bool>("percent-below");
        fields->m_showMarks = mod->getSettingValue<bool>("markers");
        fields->m_green = mod->getSettingValue<bool>("reached-green");
        fields->m_greenColor = mod->getSettingValue<ccColor3B>("reached-color");
        fields->m_chime = mod->getSettingValue<bool>("chime");
        fields->m_chimeVolume = mod->getSettingValue<double>("chime-volume");
        if (auto profile = linkedProfile(m_level)) {
            fields->m_tracked = true;
            auto json = file::readJson(*profile);
            if (!json) return;

            // Start objects are stable while a spawn/reset percentage can be a
            // little early or late. Checkpoints must keep their own run start.
            if (m_startPosObject && m_levelLength > 0.f
                && !(m_isPracticeMode && m_checkpointArray && m_checkpointArray->count())) {
                float x = m_startPosObject->getPositionX();
                float positionPercent = x / m_levelLength * 100.f;
                float levelTime = this->timeForPos({ m_levelLength, 0.f }, 0.f, 0.f, true, 0.f);
                float timePercent = levelTime > 0.f
                    ? this->timeForPos({ x, 0.f }, 0.f, 0.f, true, 0.f) / levelTime * 100.f
                    : positionPercent;
                fields->m_start = target::startObjectPercent(
                    this->getCurrentPercent(), positionPercent, timePercent
                );
            }

            fields->m_target = target::runTarget(
                json.unwrap(), fields->m_start, mod->getSettingValue<bool>("show-passed")
            );
        }
    }

    // The font's space is wider than wanted, so the glyphs are nudged apart
    // around the slash instead. Setting the string lays them out afresh.
    void padSlash(size_t slash, size_t length, float gap) {
        for (size_t i = slash; i < length; i++) {
            if (auto glyph = m_percentageLabel->getChildByTag(i)) {
                glyph->setPositionX(glyph->getPositionX() + (i == slash ? gap : gap * 2));
            }
        }

        auto size = m_percentageLabel->getContentSize();
        m_percentageLabel->setContentSize({ size.width + gap * 2, size.height });
    }

    // Centres the percentage under the bar, in levels Blitzkrieg tracks
    void stackLabel() {
        auto fields = m_fields.self();

        bool stack = fields->m_percentBelow && fields->m_tracked
            && GameManager::get()->m_showProgressBar
            && m_progressBar->isVisible() && m_percentageLabel->isVisible();

        if (!stack) {
            this->unstackLabel();
            return;
        }

        if (!fields->m_stacked) {
            fields->m_labelPos = m_percentageLabel->getPosition();
            fields->m_labelAnchor = m_percentageLabel->getAnchorPoint();
            fields->m_stacked = true;
        }

        // Both sprites carry a little empty space, so their boxes overlap slightly
        float drop = (
            m_percentageLabel->getScaledContentSize().height + m_progressBar->getScaledContentSize().height
        ) / 2 - 1.5f;

        m_percentageLabel->setAnchorPoint({ 0.5f, 0.5f });
        m_percentageLabel->setPosition(m_progressBar->getPosition() - CCPoint { 0.f, drop });
    }

    void unstackLabel() {
        auto fields = m_fields.self();
        if (!fields->m_stacked) return;

        m_percentageLabel->setAnchorPoint(fields->m_labelAnchor);
        m_percentageLabel->setPosition(fields->m_labelPos);
        fields->m_stacked = false;
    }

    // Lines on the bar where the run starts and where it has to get to
    void placeMarks() {
        auto fields = m_fields.self();
        bool show = fields->m_showMarks && fields->m_target && m_progressFill;

        if (!fields->m_startMark) {
            if (!show) return;

            float height = m_progressBar->getContentSize().height;
            fields->m_startMark = CCLayerColor::create(MARK_COLOR, MARK_WIDTH, height);
            fields->m_targetMark = CCLayerColor::create(MARK_COLOR, MARK_WIDTH, height);
            m_progressBar->addChild(fields->m_startMark, 10);
            m_progressBar->addChild(fields->m_targetMark, 10);
        }

        fields->m_startMark->setVisible(show);
        fields->m_targetMark->setVisible(show);
        if (!show) return;

        float left = m_progressFill->getPositionX() - MARK_WIDTH / 2;
        fields->m_startMark->setPositionX(left + m_progressWidth * fields->m_start / 100.f);
        fields->m_targetMark->setPositionX(left + m_progressWidth * *fields->m_target / 100.f);
    }

    void restoreLabel() {
        auto fields = m_fields.self();
        if (!m_percentageLabel) return;

        this->unstackLabel();
        if (fields->m_styled) {
            if (std::string_view(m_percentageLabel->getFntFile()) != fields->m_originalFont) {
                m_percentageLabel->setFntFile(fields->m_originalFont.c_str());
            }
            m_percentageLabel->setColor(fields->m_originalColor);
            fields->m_styled = false;
        }

        // Other mods should receive the game's percentage, without our suffix.
        if (std::string_view(m_percentageLabel->getString()) == fields->m_written) {
            m_percentageLabel->setString(fields->m_base.c_str());
        }
        fields->m_written.clear();
    }

    void colorLabel(bool green) {
        if (!green) return;
        auto fields = m_fields.self();
        fields->m_originalFont = m_percentageLabel->getFntFile();
        fields->m_originalColor = m_percentageLabel->getColor();
        fields->m_styled = true;

        // goldFont's glyphs are already yellow, so tinting them green cannot
        // produce the requested colour. Use white glyphs until the next update.
        if (fields->m_originalFont.find("goldFont") != std::string::npos) {
            m_percentageLabel->setFntFile("bigFont.fnt");
        }
        m_percentageLabel->setColor(fields->m_greenColor);
    }

    void toggleProgressbar() {
        // The game lays the label out again from its own position
        if (m_percentageLabel && m_progressBar) this->unstackLabel();

        PlayLayer::toggleProgressbar();
    }

    void updateProgressbar() {
        this->restoreLabel();
        PlayLayer::updateProgressbar();

        if (!m_percentageLabel || !m_progressBar) return;

        auto fields = m_fields.self();
        if (fields->m_resetting) return;
        if (fields->m_pending) this->refreshTarget();

        bool reached = fields->m_target && (fields->m_chimed
            || this->getCurrentPercent() + RUN_EPS >= *fields->m_target);
        if (reached && !fields->m_chimed) {
            fields->m_chimed = true;
            if (fields->m_chime) playChime(fields->m_chimeVolume);
        }
        // Font changes rebuild the glyphs; apply them before slash spacing.
        this->colorLabel(reached && fields->m_green);
        std::string current = m_percentageLabel->getString();

        fields->m_base = current;

        // A run that has reached its target is done, so only the percentage stays
        bool showTarget = fields->m_target && !reached;
        auto text = showTarget
            ? fmt::format("{}/{:.{}f}%", fields->m_base, *fields->m_target, fields->m_decimals)
            : fields->m_base;

        if (text != current) {
            m_percentageLabel->setString(text.c_str());
            if (showTarget) this->padSlash(fields->m_base.size(), text.size(), fields->m_slashGap);
        }
        fields->m_written = std::move(text);

        this->stackLabel();

        if (fields->m_marksDirty) {
            this->placeMarks();
            fields->m_marksDirty = false;
        }
    }
};
