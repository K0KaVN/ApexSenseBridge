#pragma once

#include <algorithm>
#include <chrono>
#include <optional>
#include <thread>

namespace asb::cli {

enum class DeviceOpenFailure {
    Other,
    IdentityUnanswered,
};

struct DeviceOpenRetryPolicy {
    std::chrono::milliseconds window{4000};
    std::chrono::milliseconds unansweredWindow{8000};
    std::chrono::milliseconds delay{100};
    std::chrono::milliseconds unansweredDelay{250};
};

[[nodiscard]] constexpr std::optional<std::chrono::milliseconds> nextDeviceOpenDelay(
    const DeviceOpenRetryPolicy& policy, bool identityUnansweredSeen,
    std::chrono::milliseconds elapsed) noexcept {
    const auto delay = identityUnansweredSeen ? policy.unansweredDelay : policy.delay;
    const auto window = identityUnansweredSeen ? policy.unansweredWindow : policy.window;
    if (elapsed + delay >= window) return std::nullopt;
    return delay;
}

template <typename Stop>
void sleepUnlessStopped(std::chrono::milliseconds duration, Stop&& stopRequested) {
    constexpr std::chrono::milliseconds slice{50};
    auto remaining = duration;
    while (remaining.count() > 0 && !stopRequested()) {
        const auto step = (std::min)(remaining, slice);
        std::this_thread::sleep_for(step);
        remaining -= step;
    }
}

template <typename Attempt, typename Stop, typename Now, typename Sleep>
[[nodiscard]] bool retryDeviceOpen(const DeviceOpenRetryPolicy& policy, Attempt&& attempt,
                                   Stop&& stopRequested, Now&& now, Sleep&& sleepFor) {
    const auto started = now();
    bool identityUnansweredSeen = false;
    for (;;) {
        DeviceOpenFailure failure = DeviceOpenFailure::Other;
        if (attempt(failure)) return true;
        if (stopRequested()) return false;
        identityUnansweredSeen = identityUnansweredSeen ||
                                 failure == DeviceOpenFailure::IdentityUnanswered;
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now() - started);
        const auto delay = nextDeviceOpenDelay(policy, identityUnansweredSeen, elapsed);
        if (!delay) return false;
        sleepFor(*delay);
        if (stopRequested()) return false;
    }
}

template <typename Attempt, typename Stop>
[[nodiscard]] bool retryDeviceOpen(const DeviceOpenRetryPolicy& policy, Attempt&& attempt,
                                   Stop&& stopRequested) {
    return retryDeviceOpen(
        policy, attempt, stopRequested,
        [] { return std::chrono::steady_clock::now(); },
        [&stopRequested](std::chrono::milliseconds delay) {
            sleepUnlessStopped(delay, stopRequested);
        });
}

}
