#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <Windows.h>

namespace forcevv::dx {

constexpr std::size_t MAX_ADAPTERS = 8;

struct AdapterInfo {
    char name[128]{};
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
    AdapterInfo adapters[MAX_ADAPTERS]{};
    std::size_t adapterCount{};
};

ProbeResult probeD3D12Support();
std::string formatFailureReport(const ProbeResult& result);

}
