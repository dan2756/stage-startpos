#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <cvolton.level-id-api/include/EditorIDs.hpp>

using namespace geode::prelude;

namespace {
    // Blitzkrieg has no API, so its state is read the way it stores it: the
    // level's profile link in its saved values, the stages in its save dir.
    constexpr auto BLITZKRIEG_ID = "zhulis.blitzkrieg";

    // The tolerance Blitzkrieg matches a run's start against a range with
    constexpr float START_EPS = 0.01f;

    constexpr ccColor4B MARK_COLOR = { 255, 255, 255, 255 };
    constexpr float MARK_WIDTH = 1.5f;

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

    // The percent a run starting at `start` has to reach in the stage Blitzkrieg is on
    std::optional<float> runTarget(std::filesystem::path const& profilePath, float start, bool showPassed) {
        auto profile = file::readJson(profilePath);
        if (!profile) return std::nullopt;

        auto const json = std::move(profile).unwrap();

        for (auto const& stage : json["data"]["stages"]) {
            bool open = false;
            bool targetPassed = false;
            std::optional<float> target;

            for (auto const& range : stage["ranges"]) {
                if (!range["consider"].asBool().unwrapOr(true)) continue;

                bool passed = range["checked"].asBool().unwrapOr(false);
                if (!passed) open = true;

                float from = range["from"].asDouble().unwrapOr(0.0);
                if (std::abs(from - start) >= START_EPS) continue;
                if (passed && !showPassed) continue;

                // A run still to pass wins over a passed one, then the nearer goal
                float to = range["to"].asDouble().unwrapOr(0.0);
                if (!target || (targetPassed && !passed) || (targetPassed == passed && to < *target)) {
                    target = to;
                    targetPassed = passed;
                }
            }

            // Blitzkrieg only tracks the first stage that still has a run to pass
            if (open) return target;
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
    };

    static void onModify(auto& self) {
        // The target goes after whatever other mods turn the percentage into
        if (!self.setHookPriorityPost("PlayLayer::updateProgressbar", Priority::VeryLate)) {
            log::warn("Failed to set hook priority.");
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        auto fields = m_fields.self();
        fields->m_tracked = false;
        fields->m_target = std::nullopt;
        fields->m_marksDirty = true;

        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("enabled") || m_level->isPlatformer()) return;

        fields->m_decimals = mod->getSettingValue<int64_t>("decimals");
        fields->m_slashGap = mod->getSettingValue<double>("slash-gap");
        fields->m_percentBelow = mod->getSettingValue<bool>("percent-below");
        fields->m_showMarks = mod->getSettingValue<bool>("markers");
        fields->m_start = this->getCurrentPercent();

        if (auto profile = linkedProfile(m_level)) {
            fields->m_tracked = true;
            fields->m_target = runTarget(
                *profile, fields->m_start, mod->getSettingValue<bool>("show-passed")
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

    void toggleProgressbar() {
        // The game lays the label out again from its own position
        if (m_percentageLabel && m_progressBar) this->unstackLabel();

        PlayLayer::toggleProgressbar();
    }

    void updateProgressbar() {
        PlayLayer::updateProgressbar();

        if (!m_percentageLabel || !m_progressBar) return;

        auto fields = m_fields.self();
        std::string current = m_percentageLabel->getString();

        // Anything but our own text is a fresh percentage from the game
        if (current != fields->m_written) fields->m_base = current;

        auto text = fields->m_target
            ? fmt::format("{}/{:.{}f}%", fields->m_base, *fields->m_target, fields->m_decimals)
            : fields->m_base;

        if (text != current) {
            m_percentageLabel->setString(text.c_str());
            if (fields->m_target) this->padSlash(fields->m_base.size(), text.size(), fields->m_slashGap);
        }
        fields->m_written = std::move(text);

        this->stackLabel();

        if (fields->m_marksDirty) {
            this->placeMarks();
            fields->m_marksDirty = false;
        }
    }
};
