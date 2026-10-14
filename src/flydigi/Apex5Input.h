#pragma once

#include "dualsense/DualSenseInput.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>

namespace asb::flydigi {

// Decodes the Apex 5 NewXInput operator report (command 0xEF) carried by the
// vendor HID interface. This stream remains available while the controller's
// ordinary XInput output is disabled, allowing a single visible DualSense to
// carry every standard control plus its gyroscope and accelerometer.
[[nodiscard]] std::optional<dualsense::DualSenseInputState>
decodeApex5InputReport(std::span<const std::uint8_t> report,
                       std::uint8_t batteryPercent = 100,
                       std::uint8_t chargeState = 0) noexcept;

// Replaces the raw controls from the vendor stream with the controller's
// mapped game-controller HID state. Space Station stores its profile onboard,
// so this HID collection is the source of truth for standard controls. The
// higher-level composer uses the vendor stream to recover confirmed simultaneous
// LT/RT presses plus PS, motion and battery data.
void mergeApex5MappedControls(
    dualsense::DualSenseInputState& vendorState,
    const dualsense::DualSenseInputState& mappedState) noexcept;

[[nodiscard]] dualsense::DualSenseInputState composeApex5InputState(
    const dualsense::DualSenseInputState& mappedState,
    const std::optional<dualsense::DualSenseInputState>& vendorState,
    std::uint8_t batteryPercent,
    std::uint8_t chargeState) noexcept;

class Apex5InputComposer {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::uint8_t kIndependentEngageFloor = 8;
    static constexpr std::uint8_t kIndependentReleaseFloor = 4;
    static constexpr auto kTriggerFreshness = std::chrono::milliseconds(50);
    static constexpr int kStaleTriggerAgreement = 24;

    [[nodiscard]] dualsense::DualSenseInputState compose(
        const dualsense::DualSenseInputState& mappedState,
        const std::optional<dualsense::DualSenseInputState>& vendorState,
        Clock::duration vendorTriggerAge,
        std::uint8_t batteryPercent,
        std::uint8_t chargeState) noexcept;

    [[nodiscard]] bool independentTriggers() const noexcept { return independent_; }
    [[nodiscard]] std::uint64_t triggerSourceSwitches() const noexcept { return switches_; }

private:
    bool independent_ = false;
    std::uint64_t switches_ = 0;
};

} // namespace asb::flydigi
