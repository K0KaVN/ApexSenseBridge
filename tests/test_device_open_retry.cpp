#ifdef NDEBUG
#undef NDEBUG
#endif

#include "cli/DeviceOpenRetry.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <optional>
#include <vector>

namespace {

using asb::cli::DeviceOpenFailure;
using asb::cli::DeviceOpenRetryPolicy;
using asb::cli::nextDeviceOpenDelay;
using asb::cli::retryDeviceOpen;
using std::chrono::milliseconds;

struct FakeClock {
    milliseconds elapsed{0};
    std::chrono::steady_clock::time_point origin = std::chrono::steady_clock::time_point{};
    std::chrono::steady_clock::time_point now() const { return origin + elapsed; }
};

struct Run {
    int attempts = 0;
    int sleeps = 0;
    bool result = false;
    milliseconds elapsed{0};
};

Run simulate(const std::vector<DeviceOpenFailure>& failures, milliseconds attemptCost,
             int succeedOnAttempt = -1, int stopAfterAttempt = -1,
             int stopDuringSleep = -1) {
    FakeClock clock;
    Run run;
    bool stop = false;
    run.result = retryDeviceOpen(
        DeviceOpenRetryPolicy{},
        [&](DeviceOpenFailure& failure) {
            ++run.attempts;
            clock.elapsed += attemptCost;
            if (run.attempts == stopAfterAttempt) stop = true;
            if (run.attempts == succeedOnAttempt) return true;
            const auto index = static_cast<std::size_t>(run.attempts - 1);
            failure = index < failures.size() ? failures[index] : failures.back();
            return false;
        },
        [&] { return stop; },
        [&] { return clock.now(); },
        [&](milliseconds delay) {
            ++run.sleeps;
            clock.elapsed += delay;
            if (run.sleeps == stopDuringSleep) stop = true;
        });
    run.elapsed = clock.elapsed;
    return run;
}

void testDecisionHelper() {
    const DeviceOpenRetryPolicy policy;
    assert(nextDeviceOpenDelay(policy, false, milliseconds(0)) == milliseconds(100));
    assert(nextDeviceOpenDelay(policy, false, milliseconds(3800)) == milliseconds(100));
    assert(!nextDeviceOpenDelay(policy, false, milliseconds(3900)));
    assert(!nextDeviceOpenDelay(policy, false, milliseconds(9000)));
    assert(nextDeviceOpenDelay(policy, true, milliseconds(0)) == milliseconds(250));
    assert(nextDeviceOpenDelay(policy, true, milliseconds(7700)) == milliseconds(250));
    assert(!nextDeviceOpenDelay(policy, true, milliseconds(7750)));
    assert(!nextDeviceOpenDelay(policy, true, milliseconds(20000)));
    assert(policy.window == milliseconds(4000));
    assert(policy.unansweredWindow >= milliseconds(8000) &&
           policy.unansweredWindow <= milliseconds(10000));
}

void testUnansweredIdentityKeepsReopeningWithinBoundedWindow() {
    const auto run = simulate({DeviceOpenFailure::IdentityUnanswered}, milliseconds(3000));
    assert(!run.result);
    assert(run.attempts == 3);
    assert(run.sleeps == 2);
    assert(run.elapsed >= milliseconds(8000) && run.elapsed <= milliseconds(10000));
}

void testOtherFailuresKeepTheOriginalShortWindow() {
    const auto run = simulate({DeviceOpenFailure::Other}, milliseconds(3000));
    assert(!run.result);
    assert(run.attempts == 2);
    assert(run.elapsed <= milliseconds(6200));

    const auto quick = simulate({DeviceOpenFailure::Other}, milliseconds(1));
    assert(!quick.result);
    assert(quick.elapsed >= milliseconds(3900) && quick.elapsed <= milliseconds(4100));
    assert(quick.attempts > 30);
}

void testUnansweredIdentityExtendsLaterFailuresToo() {
    const auto run = simulate(
        {DeviceOpenFailure::IdentityUnanswered, DeviceOpenFailure::Other}, milliseconds(1500));
    assert(!run.result);
    assert(run.elapsed > milliseconds(4100));
    assert(run.elapsed <= milliseconds(10000));
    assert(run.attempts >= 4);
}

void testRecoversWhenTheInterfaceAnswersLater() {
    const auto run = simulate({DeviceOpenFailure::IdentityUnanswered}, milliseconds(3000), 3);
    assert(run.result);
    assert(run.attempts == 3);
    assert(run.sleeps == 2);

    const auto first = simulate({DeviceOpenFailure::Other}, milliseconds(10), 1);
    assert(first.result && first.attempts == 1 && first.sleeps == 0);
}

void testStopRequestEndsRetriesImmediately() {
    const auto afterAttempt = simulate(
        {DeviceOpenFailure::IdentityUnanswered}, milliseconds(3000), -1, 1);
    assert(!afterAttempt.result);
    assert(afterAttempt.attempts == 1 && afterAttempt.sleeps == 0);

    const auto duringSleep = simulate(
        {DeviceOpenFailure::IdentityUnanswered}, milliseconds(3000), -1, -1, 1);
    assert(!duringSleep.result);
    assert(duringSleep.attempts == 1 && duringSleep.sleeps == 1);
}

void testSleepHelperHonoursStop() {
    const auto begin = std::chrono::steady_clock::now();
    asb::cli::sleepUnlessStopped(milliseconds(2000), [] { return true; });
    assert(std::chrono::steady_clock::now() - begin < milliseconds(500));

    int polls = 0;
    const auto start = std::chrono::steady_clock::now();
    asb::cli::sleepUnlessStopped(milliseconds(120), [&] { return ++polls > 100; });
    const auto slept = std::chrono::steady_clock::now() - start;
    assert(slept >= milliseconds(100) && slept < milliseconds(1000));

    int attempts = 0;
    assert(retryDeviceOpen(DeviceOpenRetryPolicy{},
                           [&](DeviceOpenFailure&) { return ++attempts == 2; },
                           [] { return false; }));
    assert(attempts == 2);
}

}

int main() {
    testDecisionHelper();
    testUnansweredIdentityKeepsReopeningWithinBoundedWindow();
    testOtherFailuresKeepTheOriginalShortWindow();
    testUnansweredIdentityExtendsLaterFailuresToo();
    testRecoversWhenTheInterfaceAnswersLater();
    testStopRequestEndsRetriesImmediately();
    testSleepHelperHonoursStop();
    std::cout << "Device open retry tests passed\n";
    return 0;
}
