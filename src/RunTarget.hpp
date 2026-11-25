#pragma once

#include <matjson.hpp>
#include <cmath>
#include <optional>

namespace target {
    constexpr float RUN_EPS = 0.01f;

    // Keep the current stage even when this start has no remaining run in it.
    inline std::optional<float> runTarget(matjson::Value const& profile, float start, bool showPassed) {
        if (!std::isfinite(start)) return std::nullopt;

        for (auto const& stage : profile["data"]["stages"]) {
            if (stage["checked"].asBool().unwrapOr(false)) continue;

            bool open = false;
            bool targetPassed = false;
            std::optional<float> result;

            for (auto const& range : stage["ranges"]) {
                if (!range["consider"].asBool().unwrapOr(true)) continue;

                bool passed = range["checked"].asBool().unwrapOr(false);
                if (!passed) open = true;

                float from = range["from"].asDouble().unwrapOr(0.0);
                if (std::abs(from - start) >= RUN_EPS) continue;
                if (passed && !showPassed) continue;

                float to = range["to"].asDouble().unwrapOr(0.0);
                if (!result || (targetPassed && !passed) || (targetPassed == passed && to < *result)) {
                    result = to;
                    targetPassed = passed;
                }
            }

            if (open) return result;
        }

        return std::nullopt;
    }

    // Blitzkrieg generates ranges from the start object, not the player's spawn
    // position. Select the same percentage convention as the current attempt.
    inline float startObjectPercent(float runtime, float positionPercent, float timePercent) {
        if (!std::isfinite(timePercent)) return positionPercent;
        return std::abs(runtime - positionPercent) <= std::abs(runtime - timePercent)
            ? positionPercent : timePercent;
    }
}
