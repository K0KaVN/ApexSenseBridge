#ifdef NDEBUG
#undef NDEBUG
#endif

#include "platform/PhysicalInputActivity.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>

int main() {
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;
    namespace button = asb::dualsense::button;
    const auto started = Clock::time_point{};

    asb::platform::TriggerOscillationDetector flapping;
    auto at = started;
    for (int sample = 0; sample < 400; ++sample) {
        flapping.observe(255, at);
        at += 1ms;
    }
    assert(flapping.reversals() == 0);
    for (int sample = 0; sample < 10; ++sample) {
        flapping.observe(sample % 2 == 0 ? 247 : 255, at);
        at += 3ms;
    }
    assert(flapping.reversals() == 9);

    asb::platform::TriggerOscillationDetector staleDip;
    staleDip.observe(255, started);
    staleDip.observe(255, started + 200ms);
    staleDip.observe(100, started + 203ms);
    staleDip.observe(255, started + 206ms);
    staleDip.observe(255, started + 900ms);
    assert(staleDip.reversals() == 1);

    asb::platform::TriggerOscillationDetector slowAim;
    slowAim.observe(255, started);
    slowAim.observe(200, started + 150ms);
    slowAim.observe(255, started + 300ms);
    slowAim.observe(200, started + 450ms);
    slowAim.observe(255, started + 600ms);
    assert(slowAim.reversals() == 0);

    asb::platform::TriggerOscillationDetector tremor;
    for (int sample = 0; sample < 50; ++sample) {
        tremor.observe(sample % 2 == 0 ? 255 : 249, started + sample * 3ms);
    }
    assert(tremor.reversals() == 0);

    asb::platform::TriggerOscillationDetector lightPress;
    for (int sample = 0; sample < 50; ++sample) {
        lightPress.observe(sample % 2 == 0 ? 60 : 100, started + sample * 3ms);
    }
    assert(lightPress.reversals() == 0);

    asb::platform::TriggerOscillationDetector rapidTaps;
    for (int sample = 0; sample < 20; ++sample) {
        rapidTaps.observe(sample % 2 == 0 ? 0 : 255, started + sample * 40ms);
    }
    assert(rapidTaps.reversals() == 0);

    asb::platform::ButtonBounceDetector bounce;
    bounce.observe(0, 0, started);
    bounce.observe(button::kCross, 0, started + 10ms);
    bounce.observe(0, 0, started + 110ms);
    bounce.observe(button::kCross, 0, started + 125ms);
    assert(bounce.bounces() == 1 && bounce.bouncedButtons() == button::kCross);
    bounce.observe(0, 0, started + 200ms);
    bounce.observe(button::kCross, 0, started + 260ms);
    assert(bounce.bounces() == 1);
    bounce.observe(0, 0, started + 300ms);
    bounce.observe(button::kCross, 0, started + 340ms);
    assert(bounce.bounces() == 2);
    bounce.observe(button::kCross, 0x01, started + 400ms);
    bounce.observe(button::kCross, 0, started + 450ms);
    bounce.observe(button::kCross | button::kCircle, 0x01, started + 455ms);
    assert(bounce.bounces() == 3 && bounce.bouncedDpad() == 0x01);
    assert(bounce.bouncedButtons() == button::kCross);
    bounce.observe(button::kCross | button::kL2, 0, started + 500ms);
    bounce.observe(button::kCross, 0, started + 503ms);
    bounce.observe(button::kCross | button::kL2, 0, started + 506ms);
    assert(bounce.bounces() == 3 && bounce.triggerBounces() == 1);
    assert(bounce.bouncedButtons() == (button::kCross | button::kL2));

    asb::platform::ButtonBounceDetector heldAtStart;
    heldAtStart.observe(button::kOptions, 0, started);
    heldAtStart.observe(button::kOptions, 0, started + 1s);
    assert(heldAtStart.bounces() == 0);
    heldAtStart.observe(0, 0, started + 2s);
    heldAtStart.observe(button::kOptions, 0, started + 2s + 39ms);
    assert(heldAtStart.bounces() == 1);

    asb::platform::PhysicalInputActivityTracker tracker;
    asb::dualsense::DualSenseInputState state{};
    tracker.observe(state, started);
    state.lx = 0x90;
    tracker.observe(state, started + 1ms);
    state.l2 = 100;
    state.buttons = button::kL2;
    tracker.observe(state, started + 2ms);
    state.r2 = 90;
    tracker.observe(state, started + 3ms);
    state.dpad = 0x02;
    tracker.observe(state, started + 4ms);
    state.gyroX = 5;
    tracker.observe(state, started + 5ms);
    state.accelZ = -3;
    tracker.observe(state, started + 6ms);
    tracker.observe(state, started + 7ms);
    state.buttons = 0;
    tracker.observe(state, started + 8ms);
    state.buttons = button::kL2;
    tracker.observe(state, started + 9ms);
    const auto activity = tracker.stats();
    assert(activity.stickChanges == 1);
    assert(activity.l2Changes == 1 && activity.r2Changes == 1);
    assert(activity.buttonChanges == 3);
    assert(activity.dpadChanges == 1);
    assert(activity.imuChanges == 2);
    assert(activity.triggerButtonBounces == 1 && activity.buttonBounces == 0);
    assert(activity.bouncedButtons == button::kL2 && activity.bouncedDpad == 0);
    assert(activity.l2Oscillations == 0 && activity.r2Oscillations == 0);

    asb::platform::PhysicalInputActivityStats total{};
    total.l2Changes = 4;
    total.bouncedButtons = button::kCross;
    total.bouncedDpad = 0x01;
    asb::platform::accumulatePhysicalInputActivity(total, activity);
    assert(total.l2Changes == 5 && total.imuChanges == 2);
    assert(total.triggerButtonBounces == 1);
    assert(total.bouncedButtons == (button::kCross | button::kL2));
    assert(total.bouncedDpad == 0x01);

    asb::platform::VendorStreamGapTracker gaps(50ms);
    assert(gaps.maximumGapMilliseconds() == 0 && gaps.staleGaps() == 0);
    gaps.observe(started + 10s);
    gaps.observe(started + 10s + 3ms);
    gaps.observe(started + 10s + 6ms);
    gaps.observe(started + 10s + 70ms);
    gaps.observe(started + 10s + 73ms);
    gaps.observe(started + 10s + 123ms);
    assert(gaps.maximumGapMilliseconds() == 64);
    assert(gaps.staleGaps() == 1);
    gaps.observe(started + 10s + 400ms);
    assert(gaps.maximumGapMilliseconds() == 277 && gaps.staleGaps() == 2);
    return 0;
}
