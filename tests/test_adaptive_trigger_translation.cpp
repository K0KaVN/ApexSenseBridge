#ifdef NDEBUG
#undef NDEBUG
#endif

#include "dualsense/AdaptiveTriggerTranslation.h"

#include <array>
#include <cassert>

namespace {

void expectCommand(const std::array<std::uint8_t, 11>& effect,
                   asb::TriggerMode mode,
                   const std::array<std::uint8_t, 5>& params) {
    for (const auto side : {asb::TriggerSide::Left, asb::TriggerSide::Right}) {
        for (const auto motor : {std::uint8_t{0}, std::uint8_t{61}, std::uint8_t{255}}) {
            const auto command = asb::dualsense::translateAdaptiveTrigger(side, effect, motor);
            assert(command && command->side == side);
            assert(command->mode == mode);
            assert(command->params == params);
        }
    }
}

void expectUnsupported(const std::array<std::uint8_t, 11>& effect) {
    for (const auto side : {asb::TriggerSide::Left, asb::TriggerSide::Right}) {
        assert(!asb::dualsense::translateAdaptiveTrigger(side, effect, 0));
    }
}

}

int main() {
    using asb::TriggerMode;

    expectCommand({1, 25, 40}, TriggerMode::Race, {25, 40, 0, 0, 0});
    expectCommand({2, 25, 90, 40}, TriggerMode::SniperBreak, {25, 90, 40, 0, 0});
    expectCommand({6, 35, 40, 25}, TriggerMode::RecoilRattle, {25, 40, 40, 35, 0});
    expectCommand({5}, TriggerMode::Normal, {});

    expectCommand({0x21, 0xFC, 0x03}, TriggerMode::Race, {38, 8, 0, 0, 0});
    expectCommand({0x21, 0x80, 0, 0, 0, 0xE0}, TriggerMode::Race, {134, 64, 0, 0, 0});
    expectCommand({0x21, 0, 0x02, 0, 0, 0, 0x38}, TriggerMode::Race, {173, 64, 0, 0, 0});
    expectCommand({0x21, 0x24, 0, 0x40, 0x80, 0x03}, TriggerMode::Race, {38, 64, 0, 0, 0});
    expectCommand({0x21, 0x01, 0, 0, 0, 0, 0x38}, TriggerMode::Race, {0, 8, 0, 0, 0});
    expectCommand({0x21}, TriggerMode::Normal, {});

    expectCommand({0x25, 0x84, 0, 3}, TriggerMode::SniperBreak, {38, 96, 32, 0, 0});
    expectCommand({0x25, 0x14, 0, 3}, TriggerMode::SniperBreak, {38, 39, 32, 0, 0});
    expectCommand({0x25, 0x84}, TriggerMode::SniperBreak, {38, 96, 8, 0, 0});
    expectCommand({0x25, 0x84, 0, 7}, TriggerMode::SniperBreak, {38, 96, 64, 0, 0});
    expectCommand({0x25, 0x01, 0x02, 3}, TriggerMode::SniperBreak, {0, 173, 32, 0, 0});
    expectCommand({0x25, 0, 0x03, 3}, TriggerMode::SniperBreak, {154, 19, 32, 0, 0});
    for (unsigned startZone = 0; startZone < 9; ++startZone) {
        for (unsigned endZone = startZone + 1; endZone < 10; ++endZone) {
            const auto mask = (1U << startZone) | (1U << endZone);
            const auto startStroke = (startZone * 192U + 5U) / 10U;
            const auto endStroke = (endZone * 192U + 5U) / 10U;
            expectCommand({0x25, static_cast<std::uint8_t>(mask & 0xFFU),
                           static_cast<std::uint8_t>(mask >> 8U), 3},
                          TriggerMode::SniperBreak,
                          {static_cast<std::uint8_t>(startStroke),
                           static_cast<std::uint8_t>(endStroke - startStroke), 32, 0, 0});
        }
    }
    expectCommand({0x25}, TriggerMode::Normal, {});
    expectCommand({0x25, 0x04, 0, 3}, TriggerMode::SniperBreak, {38, 20, 32, 0, 0});
    expectCommand({0x25, 0, 0x02, 3}, TriggerMode::SniperBreak, {154, 19, 32, 0, 0});
    expectCommand({0x25, 0x94}, TriggerMode::SniperBreak, {38, 96, 8, 0, 0});

    expectCommand({0x22, 0x84, 0, 0x2B}, TriggerMode::Race, {38, 32, 0, 0, 0});
    expectCommand({0x22, 0x01, 0x02, 0x39}, TriggerMode::Race, {0, 16, 0, 0, 0});
    expectCommand({0x22, 0x04, 0, 0x07}, TriggerMode::Race, {38, 64, 0, 0, 0});
    expectCommand({0x22}, TriggerMode::Normal, {});
    expectCommand({0x22, 0, 0, 0x38}, TriggerMode::Normal, {});

    expectCommand({0x22, 0x01, 0x02, 0x18}, TriggerMode::Race, {0, 8, 0, 0, 0});
    expectCommand({0x22, 0x01, 0x01, 0x38}, TriggerMode::SniperBreak, {0, 154, 8, 0, 0});
    expectCommand({0x22, 0x01, 0x02, 0x39}, TriggerMode::Race, {0, 16, 0, 0, 0});
    expectCommand({0x22, 0x80, 0x02, 0x08}, TriggerMode::Race, {134, 8, 0, 0, 0});
    expectCommand({0x22, 0x40, 0x01, 0x21}, TriggerMode::Race, {115, 16, 0, 0, 0});
    expectCommand({0x22, 0x04, 0x02, 0x32}, TriggerMode::Race, {38, 24, 0, 0, 0});
    expectCommand({0x22, 0x04, 0x01, 0x3A}, TriggerMode::SniperBreak, {38, 116, 24, 0, 0});
    expectCommand({0x21, 0x80, 0x03, 0, 0, 0, 0, 0, 0, 0, 0}, TriggerMode::Race, {134, 8, 0, 0, 0});

    expectCommand({0x22, 0x01, 0x01, 0x30}, TriggerMode::SniperBreak, {0, 154, 8, 0, 0});
    expectCommand({0x22, 0x01, 0x01, 0x28}, TriggerMode::Race, {0, 8, 0, 0, 0});
    expectCommand({0x22, 0x01, 0x01, 0x3F}, TriggerMode::SniperBreak, {0, 154, 64, 0, 0});
    expectCommand({0x22, 0x01, 0x01, 0x07}, TriggerMode::Race, {0, 64, 0, 0, 0});
    expectCommand({0x22, 0x01, 0x02, 0x3F}, TriggerMode::Race, {0, 64, 0, 0, 0});
    expectCommand({0x22, 0x08, 0x01, 0x38}, TriggerMode::SniperBreak, {58, 96, 8, 0, 0});
    expectCommand({0x22, 0x18, 0, 0x38}, TriggerMode::SniperBreak, {58, 19, 8, 0, 0});
    expectCommand({0x22, 0x80, 0x01, 0x30}, TriggerMode::SniperBreak, {134, 20, 8, 0, 0});
    expectCommand({0x22, 0x00, 0x03, 0x38}, TriggerMode::Race, {154, 8, 0, 0, 0});
    expectCommand({0x22, 0x01, 0x01, 0xF8}, TriggerMode::SniperBreak, {0, 154, 8, 0, 0});

    expectCommand({0x22, 0x04, 0, 0x38}, TriggerMode::Race, {38, 8, 0, 0, 0});
    expectCommand({0x22, 0x00, 0x02, 0x38}, TriggerMode::Race, {173, 8, 0, 0, 0});
    expectCommand({0x22, 0x52, 0, 0x38}, TriggerMode::SniperBreak, {19, 96, 8, 0, 0});
    expectCommand({0x22, 0x52, 0x02, 0x38}, TriggerMode::Race, {19, 8, 0, 0, 0});
    expectCommand({0x22, 0xFF, 0x00, 0x38}, TriggerMode::SniperBreak, {0, 134, 8, 0, 0});
    expectCommand({0x22, 0xFF, 0x03, 0x38}, TriggerMode::Race, {0, 8, 0, 0, 0});

    for (unsigned startZone = 0; startZone < 9; ++startZone) {
        for (unsigned endZone = startZone + 1; endZone < 10; ++endZone) {
            const auto mask = (1U << startZone) | (1U << endZone);
            const auto startStroke = (startZone * 192U + 5U) / 10U;
            const auto endStroke = (endZone * 192U + 5U) / 10U;
            for (unsigned snap = 1; snap <= 8; ++snap) {
                for (unsigned draw = 1; draw <= 8; ++draw) {
                    const auto force = static_cast<std::uint8_t>((snap - 1U) << 3U | (draw - 1U));
                    const std::array<std::uint8_t, 11> effect{
                        0x22, static_cast<std::uint8_t>(mask & 0xFFU),
                        static_cast<std::uint8_t>(mask >> 8U), force};
                    const auto resistance = static_cast<std::uint8_t>(draw * 8U);
                    if (endZone < 9 && snap >= 7) {
                        expectCommand(effect, TriggerMode::SniperBreak,
                                      {static_cast<std::uint8_t>(startStroke),
                                       static_cast<std::uint8_t>(endStroke - startStroke),
                                       resistance, 0, 0});
                    } else {
                        expectCommand(effect, TriggerMode::Race,
                                      {static_cast<std::uint8_t>(startStroke), resistance, 0, 0, 0});
                    }
                }
            }
        }
    }

    expectCommand({0x23, 0x04, 0x02, 0x21, 20}, TriggerMode::RecoilRattle, {38, 1, 60, 20, 0});
    expectCommand({0x23, 0x04, 0x02, 0x21, 0}, TriggerMode::Normal, {});
    expectCommand({0x27, 0x03, 0, 0x3B, 30, 5}, TriggerMode::RecoilRattle, {0, 1, 60, 30, 0});
    expectCommand({0x27, 0x03, 0, 0x3B, 0, 5}, TriggerMode::Normal, {});
    expectCommand({0x11, 25, 40}, TriggerMode::Race, {25, 40, 0, 0, 0});
    expectCommand({0x12, 25, 90, 40}, TriggerMode::SniperBreak, {25, 90, 40, 0, 0});

    expectCommand({0x26, 0xF0, 0x03, 0, 0, 0, 0, 0, 0, 35},
                  TriggerMode::RecoilRattle, {77, 1, 10, 35, 0});
    expectCommand({0x26, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0x3F, 0, 0, 35},
                  TriggerMode::RecoilRattle, {0, 1, 80, 35, 0});
    expectCommand({0x26, 0xFC, 0x03, 0, 0, 0, 0, 0, 0, 120},
                  TriggerMode::RecoilRattle, {38, 1, 10, 120, 0});
    expectCommand({0x26, 0, 0x02, 0, 0, 0, 0x38, 0, 0, 255},
                  TriggerMode::RecoilRattle, {173, 1, 80, 255, 0});
    // Horizon held bow: low uniform amplitude at 4 Hz must stay a gentle pulse.
    expectCommand({0x26, 0xFF, 0x03, 0x49, 0x92, 0x24, 0x09, 0, 0, 4},
                  TriggerMode::RecoilRattle, {0, 1, 10, 4, 0});
    // Graded ramp starts where it reaches half of peak and averages from there.
    expectCommand({0x26, 0xFF, 0x03, 0x40, 0x22, 0x6D, 0x24, 0, 0, 35},
                  TriggerMode::RecoilRattle, {77, 1, 40, 35, 0});
    expectCommand({0x26, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0x3F, 0, 0, 0, 35},
                  TriggerMode::Normal, {});
    expectCommand({0x26, 0, 0, 0, 0, 0, 0, 0, 0, 35}, TriggerMode::Normal, {});

    expectUnsupported({0x21, 0x01, 0x04});
    expectUnsupported({0x25, 0x84, 0x80});
    expectUnsupported({0x26, 0xFF, 0x83});
    expectUnsupported({99});
    expectUnsupported({0xFC});
    expectUnsupported({0x22, 0xFF, 0x83});
    expectUnsupported({0});
}
