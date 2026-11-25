#include "../src/RunTarget.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>

void check(bool passed, char const* name) {
    if (!passed) {
        std::cerr << "FAIL: " << name << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    auto profile = matjson::parse(R"({"data":{"stages":[
        {"checked":true,"ranges":[{"from":30,"to":40,"checked":false}]},
        {"ranges":[
            {"from":30,"to":45,"checked":true},
            {"from":30,"to":65,"checked":false},
            {"from":30,"to":55,"checked":false},
            {"from":50,"to":75,"checked":true},
            {"from":60,"to":80,"checked":false,"consider":false}
        ]},
        {"ranges":[{"from":50,"to":90,"checked":false}]}
    ]}})").unwrap();

    check(target::runTarget(profile, 30.f, false) == 55.f, "nearest unchecked run in active stage");
    check(target::runTarget(profile, 30.f, true) == 55.f, "unchecked run beats a passed run");
    check(!target::runTarget(profile, 50.f, false), "hidden passed run does not jump to next stage");
    check(target::runTarget(profile, 50.f, true) == 75.f, "show passed run");
    check(!target::runTarget(profile, 60.f, true), "ignored range");
    check(!target::runTarget(profile, 31.f, true), "unmatched start");
    check(target::runTarget(profile, 30.005f, false) == 55.f, "rounding tolerance");
    check(!target::runTarget(profile, std::numeric_limits<float>::quiet_NaN(), true), "invalid start");
    check(!target::runTarget(matjson::Value(), 0.f, true), "missing profile data");

    check(target::startObjectPercent(29.05f, 29.07f, 35.f) == 29.07f, "spawn drift in position percentages");
    check(target::startObjectPercent(34.98f, 29.07f, 35.f) == 35.f, "spawn drift in time percentages");

    unsigned verified = 0;
    for (int i = 1; i < argc; ++i) {
        std::ifstream input(argv[i]);
        auto realProfile = matjson::parse(input).unwrap();
        for (auto const& stage : realProfile["data"]["stages"]) {
            if (stage["checked"].asBool().unwrapOr(false)) continue;
            bool open = false;
            for (auto const& range : stage["ranges"]) {
                if (!range["consider"].asBool().unwrapOr(true)
                    || range["checked"].asBool().unwrapOr(false)) continue;
                open = true;
                float from = range["from"].asDouble().unwrap();
                float to = range["to"].asDouble().unwrap();
                // A spawn displaced by > eps used to lose its target. The
                // selected start object must still resolve it in either mode.
                for (float drift : { -0.03f, 0.03f }) {
                    auto start = target::startObjectPercent(from + drift, from, from + 5.f);
                    auto result = target::runTarget(realProfile, start, false);
                    check(result && *result <= to, "real profile start-position switching");
                    ++verified;
                }
            }
            if (open) break;
        }
    }
    std::cout << "Run selection tests passed; " << verified << " real-profile spawn-drift cases passed.\n";
}
