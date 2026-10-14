#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace asb::platform {

inline constexpr std::chrono::milliseconds kOverlappedCancelTimeout{500};

struct OverlappedIo {
    explicit OverlappedIo(std::size_t size)
        : event(CreateEventW(nullptr, TRUE, FALSE, nullptr)), buffer(size, 0) {
        overlapped.hEvent = event;
    }
    ~OverlappedIo() {
        if (event) CloseHandle(event);
    }
    OverlappedIo(const OverlappedIo&) = delete;
    OverlappedIo& operator=(const OverlappedIo&) = delete;

    HANDLE event = nullptr;
    OVERLAPPED overlapped{};
    std::vector<std::uint8_t> buffer;
};

[[nodiscard]] inline bool cancelOverlappedIo(
    HANDLE handle, OVERLAPPED& overlapped,
    std::chrono::milliseconds timeout = kOverlappedCancelTimeout) noexcept {
    (void)CancelIoEx(handle, &overlapped);
    const auto milliseconds = timeout.count() > 0 ? static_cast<DWORD>(timeout.count()) : 0;
    return WaitForSingleObject(overlapped.hEvent, milliseconds) == WAIT_OBJECT_0;
}

}

#endif
