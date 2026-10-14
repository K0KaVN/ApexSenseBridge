#ifdef NDEBUG
#undef NDEBUG
#endif

#include "platform/HidTransport.h"
#include "platform/windows/WindowsOverlappedIo.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using TransportPtr = std::unique_ptr<asb::platform::HidTransport,
                                     void (*)(asb::platform::HidTransport*)>;

std::wstring pipeName(std::wstring_view suffix) {
    return L"\\\\.\\pipe\\ApexSenseBridge.HidTransportTest." +
           std::to_wstring(GetCurrentProcessId()) + L"." + std::wstring(suffix);
}

HANDLE createServer(const std::wstring& name) {
    return CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX,
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                            1, 4096, 4096, 0, nullptr);
}

}

int main() {
    using namespace std::chrono_literals;
    using asb::platform::HidReadStatus;
    using asb::platform::HidStringQuery;
    using asb::platform::queriesHidStrings;
    using Clock = std::chrono::steady_clock;

    assert(queriesHidStrings(HidStringQuery::KnownControllers, 0x37D7, 0x2001));
    assert(queriesHidStrings(HidStringQuery::KnownControllers, 0x054C, 0x0CE6));
    assert(queriesHidStrings(HidStringQuery::KnownControllers, 0x04B4, 0x2412));
    assert(!queriesHidStrings(HidStringQuery::KnownControllers, 0x04B4, 0x0001));
    assert(!queriesHidStrings(HidStringQuery::KnownControllers, 0x045E, 0x028E));
    assert(queriesHidStrings(HidStringQuery::AllDevices, 0x045E, 0x028E));

    const auto ioName = pipeName(L"io");
    HANDLE ioServer = createServer(ioName);
    assert(ioServer != INVALID_HANDLE_VALUE);
    HANDLE ioClient = CreateFileW(ioName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    assert(ioClient != INVALID_HANDLE_VALUE);

    asb::platform::OverlappedIo pending(8);
    assert(pending.event);
    assert(!ReadFile(ioClient, pending.buffer.data(), 8, nullptr, &pending.overlapped));
    assert(GetLastError() == ERROR_IO_PENDING);
    assert(asb::platform::cancelOverlappedIo(ioClient, pending.overlapped));
    DWORD ignored = 0;
    assert(!GetOverlappedResult(ioClient, &pending.overlapped, &ignored, FALSE));
    assert(GetLastError() == ERROR_OPERATION_ABORTED);

    asb::platform::OverlappedIo neverIssued(8);
    const auto boundedFrom = Clock::now();
    assert(!asb::platform::cancelOverlappedIo(ioClient, neverIssued.overlapped, 50ms));
    const auto bounded = Clock::now() - boundedFrom;
    assert(bounded >= 40ms && bounded < 5s);
    CloseHandle(ioClient);
    CloseHandle(ioServer);

    const auto transportName = pipeName(L"transport");
    HANDLE server = createServer(transportName);
    assert(server != INVALID_HANDLE_VALUE);
    asb::HidDeviceInfo info{};
    info.path = transportName;
    info.outputReportLength = 8;
    std::string error;
    TransportPtr transport(asb::platform::createHidTransport(info, error),
                           asb::platform::destroyHidTransport);
    assert(transport && transport->isOpen());

    std::vector<std::uint8_t> input(16, 0xEE);
    std::size_t bytesRead = 99;
    const auto timeoutFrom = Clock::now();
    assert(transport->readInputReport(input, 20ms, bytesRead, error) == HidReadStatus::Timeout);
    assert(Clock::now() - timeoutFrom < 5s);
    assert(bytesRead == 0 && input[0] == 0xEE);
    assert(transport->readInputReport(input, 0ms, bytesRead, error) == HidReadStatus::Timeout);

    const std::array<std::uint8_t, 3> reply{0x04, 0x5A, 0xA5};
    DWORD transferred = 0;
    assert(WriteFile(server, reply.data(), static_cast<DWORD>(reply.size()), &transferred, nullptr));
    assert(transferred == reply.size());
    assert(transport->readInputReport(input, 1000ms, bytesRead, error) == HidReadStatus::Data);
    assert(bytesRead == 3);
    assert(input[0] == 0x04 && input[1] == 0x5A && input[2] == 0xA5 && input[3] == 0xEE);

    const std::array<std::uint8_t, 2> request{0x03, 0x5A};
    assert(transport->writeOutputReport(request, error));
    std::array<std::uint8_t, 8> received{};
    assert(ReadFile(server, received.data(), static_cast<DWORD>(received.size()), &transferred, nullptr));
    assert(transferred == received.size());
    assert((received == std::array<std::uint8_t, 8>{0x03, 0x5A, 0, 0, 0, 0, 0, 0}));

    const std::array<std::uint8_t, 9> oversized{};
    assert(!transport->writeOutputReport(oversized, error));
    assert(error.find("output report length") != std::string::npos);

    std::vector<std::uint8_t> feature(4, 0x20);
    error.clear();
    assert(!transport->readFeatureReport(feature, error));
    assert(!error.empty() && error.find("unresponsive") == std::string::npos);
    assert(feature[0] == 0x20 && feature[3] == 0x20);

    assert(transport->writeOutputReport(request, error));
    assert(ReadFile(server, received.data(), static_cast<DWORD>(received.size()), &transferred, nullptr));
    assert(transferred == received.size() && received[0] == 0x03);

    transport.reset();
    CloseHandle(server);
    return 0;
}
