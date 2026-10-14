#ifdef NDEBUG
#undef NDEBUG
#endif

#include "cli/BridgeRuntimeSupport.h"

#include <cassert>
#include <chrono>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace {
class CountingBuffer final : public std::stringbuf {
public:
    int syncs = 0;

protected:
    int sync() override {
        ++syncs;
        return std::stringbuf::sync();
    }
};

std::size_t occurrences(const std::string& text, std::string_view needle) {
    std::size_t count = 0;
    for (auto position = text.find(needle); position != std::string::npos;
         position = text.find(needle, position + needle.size())) {
        ++count;
    }
    return count;
}

class FakeInput final : public asb::platform::PhysicalInputSource {
public:
    bool combined = true;
    bool independent = false;
    bool neutral = false;
    unsigned int polls = 0;
    unsigned int readyAfter = 3;
    asb::platform::PhysicalInputStatus status = asb::platform::PhysicalInputStatus::State;
    asb::platform::PhysicalInputStatus waitForState(
        asb::dualsense::DualSenseInputState& state,
        std::chrono::milliseconds wait, std::string& error) override {
        ++polls;
        if (status == asb::platform::PhysicalInputStatus::Timeout) std::this_thread::sleep_for(wait);
        if (status == asb::platform::PhysicalInputStatus::Error) error = "read failed";
        state.l2 = neutral ? 0 : 200;
        state.r2 = neutral ? 0 : 210;
        if (neutral) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return status;
    }
    std::string_view backendName() const noexcept override { return "fake"; }
    bool eventDriven() const noexcept override { return true; }
    asb::platform::PhysicalInputSourceStats stats() const noexcept override { return {}; }
    bool requiresIndependentTriggers() const noexcept override { return combined; }
    bool independentTriggersReady() const noexcept override { return independent && polls >= readyAfter; }
};
} // namespace

int main() {
    using namespace std::chrono_literals;

    asb::cli::MicrosecondLatencyHistogram histogram;
    assert(histogram.samples() == 0);
    assert(histogram.percentile(99) == 0);
    histogram.observe(-1us);
    histogram.observe(10us);
    histogram.observe(20us);
    histogram.observe(30us);
    histogram.observe(3ms);
    assert(histogram.samples() == 5);
    assert(histogram.percentile(50) == 20);
    assert(histogram.percentile(95) == 2000);
    assert(histogram.percentile(99) == 2000);

    asb::cli::ButtonHoldTracker holds;
    const auto start = asb::cli::ButtonHoldTracker::Clock::time_point{};
    holds.observe(false, start);
    holds.observe(true, start + 10ms);
    holds.observe(true, start + 40ms);
    holds.observe(false, start + 80ms);
    holds.observe(true, start + 100ms);
    holds.finish(start + 220ms);
    assert(holds.presses() == 2);
    assert(holds.maximumHoldMilliseconds() == 120);

    asb::dualsense::DualSenseInputState state{};
    assert(asb::cli::gameplayControlsReleased(state));
    state.l2 = 8;
    state.r2 = 8;
    assert(asb::cli::gameplayControlsReleased(state));
    state.l2 = 9;
    assert(!asb::cli::gameplayControlsReleased(state));
    state = {};
    state.buttons = 1;
    assert(!asb::cli::gameplayControlsReleased(state));
    state = {};
    state.dpad = 1;
    assert(!asb::cli::gameplayControlsReleased(state));

    assert(asb::cli::logicalProcessorCount() >= 1);

    FakeInput source;
    std::string validationError;
    asb::dualsense::DualSenseInputState latest{};
    source.combined = false;
    assert(asb::cli::validateIndependentTriggerStream(source, latest, 100ms, validationError));
    assert(source.polls == 0); // separate-trigger controllers remain unchanged
    source.combined = true;
    source.status = asb::platform::PhysicalInputStatus::Timeout;
    assert(!asb::cli::validateIndependentTriggerStream(source, latest, 5ms, validationError));
    assert(validationError.find("no live independent LT/RT") != std::string::npos);
    source.polls = 0;
    source.status = asb::platform::PhysicalInputStatus::State;
    assert(!asb::cli::validateIndependentTriggerStream(source, latest, 5ms, validationError));
    assert(source.polls > 0); // mapped reports alone cannot validate independent triggers
    source.polls = 0;
    source.independent = true;
    assert(asb::cli::validateIndependentTriggerStream(source, latest, 100ms, validationError));
    assert(source.polls == 3 && latest.l2 == 200 && latest.r2 == 210);
    assert(validationError.empty());
    source.status = asb::platform::PhysicalInputStatus::Error;
    assert(!asb::cli::validateIndependentTriggerStream(source, latest, 100ms, validationError));
    assert(validationError == "read failed");
    source.status = asb::platform::PhysicalInputStatus::Disconnected;
    assert(!asb::cli::validateIndependentTriggerStream(source, latest, 100ms, validationError));
    assert(validationError.find("disconnected") != std::string::npos);
    source.status = asb::platform::PhysicalInputStatus::State;
    source.neutral = true;
    source.independent = false;
    assert(!asb::cli::waitForPhysicalControlsReleased(source, 130ms));
    source.independent = true;
    assert(asb::cli::waitForPhysicalControlsReleased(source, 150ms));

    asb::platform::PhysicalInputSourceStats total{1, 2, 3, 4, 5, 6, 7, 8};
    asb::cli::accumulatePhysicalInputStats(total, {10, 20, 30, 40, 50, 60, 70, 80});
    assert(total.reports == 11 && total.timeouts == 22 && total.parseFailures == 33);
    assert(total.mappedReports == 44 && total.vendorReports == 55 && total.vendorStates == 66);
    assert(total.vendorParseFailures == 77 && total.vendorReadFailures == 88);
    asb::platform::PhysicalInputSourceStats retired{};
    retired.triggerSourceSwitches = 3;
    retired.vendorMaximumGapMs = 70;
    retired.vendorStaleTriggerGaps = 2;
    retired.activity.l2Oscillations = 5;
    retired.activity.bouncedButtons = asb::dualsense::button::kCross;
    asb::platform::PhysicalInputSourceStats current{};
    current.triggerSourceSwitches = 4;
    current.vendorMaximumGapMs = 40;
    current.vendorStaleTriggerGaps = 1;
    current.activity.l2Oscillations = 6;
    current.activity.bouncedButtons = asb::dualsense::button::kL2;
    asb::cli::accumulatePhysicalInputStats(retired, current);
    assert(retired.triggerSourceSwitches == 7 && retired.vendorStaleTriggerGaps == 3);
    assert(retired.vendorMaximumGapMs == 70);
    assert(retired.activity.l2Oscillations == 11);
    assert(retired.activity.bouncedButtons ==
           (asb::dualsense::button::kCross | asb::dualsense::button::kL2));
    current.vendorMaximumGapMs = 90;
    asb::cli::accumulatePhysicalInputStats(retired, current);
    assert(retired.vendorMaximumGapMs == 90);

    CountingBuffer flushed;
    std::ostream engineOutput(&flushed);
    assert((engineOutput.flags() & std::ios_base::unitbuf) == 0);
    asb::cli::configureEngineOutput(engineOutput);
    assert((engineOutput.flags() & std::ios_base::unitbuf) != 0);
    assert(engineOutput.rdbuf() == &flushed);
    engineOutput << "partial";
    assert(flushed.syncs == 1 && flushed.str() == "partial");
    engineOutput << " line\n";
    assert(flushed.syncs == 2);
    engineOutput << 42;
    assert(flushed.syncs == 3);
    assert(flushed.str() == "partial line\n42");

    std::stringstream textOutput;
    asb::cli::configureEngineOutput(textOutput);
    assert((textOutput.flags() & std::ios_base::unitbuf) != 0);
    textOutput << "engine " << 7 << '\n';
    assert(textOutput.str() == "engine 7\n");

    using StartupClock = asb::cli::StartupPhaseTracker::Clock;
    const auto origin = StartupClock::time_point{} + std::chrono::hours(1);
    std::ostringstream trace;
    asb::cli::StartupPhaseTracker tracker(trace, origin);
    assert(tracker.phase() == "starting" && !tracker.ready());
    assert(!tracker.takeStallWarning(origin + 11999ms, 12s));
    tracker.enter("identity", origin + 5ms);
    tracker.enter("calibration", origin + 1250ms);
    assert(tracker.phase() == "calibration");
    assert(trace.str() ==
           "Startup phase: identity (+5 ms)\nStartup phase: calibration (+1250 ms)\n");
    const auto stall = tracker.takeStallWarning(origin + 12500ms, 12s);
    assert(stall && tracker.stallReported());
    assert(stall->find("'calibration'") != std::string::npos);
    assert(stall->find("after 12500 ms") != std::string::npos);
    assert(stall->find("+1250 ms") != std::string::npos);
    assert(!tracker.takeStallWarning(origin + 20s, 12s));
    tracker.markReady(origin + 21s);
    assert(tracker.ready() && tracker.phase() == "ready");
    assert(trace.str().ends_with("Startup phase: ready (+21000 ms)\n"));

    std::ostringstream readyTrace;
    asb::cli::StartupPhaseTracker readyInTime(readyTrace, origin);
    readyInTime.enter("isolation", origin + 2s);
    readyInTime.markReady(origin + 3s);
    assert(!readyInTime.takeStallWarning(origin + 13s, 12s));
    assert(!readyInTime.stallReported());

    std::ostringstream stuckTrace;
    std::ostringstream stuckWarnings;
    asb::cli::StartupPhaseTracker stuck(stuckTrace, StartupClock::now());
    stuck.enter("identity");
    {
        asb::cli::StartupStallWatchdog watchdog(stuck, stuckWarnings, 20ms);
        const auto deadline = StartupClock::now() + 5s;
        while (!stuck.stallReported() && StartupClock::now() < deadline) {
            std::this_thread::sleep_for(5ms);
        }
    }
    assert(stuck.stallReported());
    assert(occurrences(stuckWarnings.str(), "Warning:") == 1);
    assert(stuckWarnings.str().find("'identity'") != std::string::npos);
    assert(stuckTrace.str().starts_with("Startup phase: identity (+"));
    assert(occurrences(stuckTrace.str(), "\n") == 1);

    std::ostringstream quickTrace;
    std::ostringstream quickWarnings;
    asb::cli::StartupPhaseTracker quick(quickTrace, StartupClock::now());
    {
        asb::cli::StartupStallWatchdog watchdog(quick, quickWarnings, 50ms);
        quick.markReady();
        std::this_thread::sleep_for(150ms);
    }
    assert(quickWarnings.str().empty() && !quick.stallReported());

    std::ostringstream idleTrace;
    std::ostringstream idleWarnings;
    asb::cli::StartupPhaseTracker idle(idleTrace, StartupClock::now());
    const auto stopStartedAt = StartupClock::now();
    {
        asb::cli::StartupStallWatchdog watchdog(idle, idleWarnings, 60s);
        watchdog.stop();
    }
    assert(StartupClock::now() - stopStartedAt < 5s);
    assert(idleWarnings.str().empty() && !idle.stallReported());
    return 0;
}
