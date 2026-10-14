#include "platform/HidTransport.h"

#include "flydigi/Apex4Protocol.h"
#include "flydigi/Apex5Protocol.h"

namespace asb::platform {

bool queriesHidStrings(HidStringQuery query,
                       std::uint16_t vendorId,
                       std::uint16_t productId) noexcept {
    constexpr std::uint16_t kSonyVendorId = 0x054C;
    return query == HidStringQuery::AllDevices || vendorId == flydigi::kVendorId ||
           vendorId == kSonyVendorId || flydigi::isApex4Product(vendorId, productId);
}

}
