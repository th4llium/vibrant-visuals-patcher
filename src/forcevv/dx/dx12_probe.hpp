#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <Windows.h>

namespace forcevv::dx {

struct AdapterInfo {
    std::string name;
    std::uint32_t vendorId{};
    std::uint32_t deviceId{};
    bool software{};
    HRESULT fl12Result{};
    HRESULT fl11Result{};
    bool supportsFl12{};
    bool supportsFl11{};
};

struct ProbeResult {
    bool allowFl11{};
    bool factoryCreated{};
    HRESULT factoryResult{};
    HRESULT enumerationResult{S_OK};
    bool hasSupportedAdapter{};
    bool usingFl11{};
    std::vector<AdapterInfo> adapters;
};

ProbeResult probeD3D12Support();
std::string formatFailureReport(const ProbeResult& result);

}
