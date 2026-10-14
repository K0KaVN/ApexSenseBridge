#pragma once

#include "core/TriggerEffect.h"

#include <array>
#include <cstdint>
#include <optional>

namespace asb::dualsense {

struct BowEmulation {
    ForceTriggerCommand draw;
    ForceTriggerCommand rupture;
    std::uint8_t ruptureAxis = 0;
    std::uint8_t rearmAxis = 0;
};

std::optional<BowEmulation> describeBowEmulation(
    TriggerSide side, const std::array<std::uint8_t, 11>& effect);

std::optional<ForceTriggerCommand> translateAdaptiveTrigger(
    TriggerSide side,
    const std::array<std::uint8_t, 11>& effect,
    std::uint8_t);

} // namespace asb::dualsense
