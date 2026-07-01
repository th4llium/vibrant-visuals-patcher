#include "forcevv/dx/dx12_probe.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <iomanip>
#include <sstream>
#include <string>

using Microsoft::WRL::ComPtr;

namespace forcevv::dx {
namespace {

std::string wideToUtf8(const wchar_t* value) {
    if (value == nullptr || value[0] == L'\0') {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), required, nullptr, nullptr);
    return result;
}

std::string hresultText(HRESULT result) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
           << static_cast<std::uint32_t>(result);
    return stream.str();
}

std::string hexText(std::uint32_t value, int width) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << std::setw(width) << std::setfill('0') << value;
    return stream.str();
}

bool envDisabled(const wchar_t* name) {
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value)));
    return length != 0 && value[0] == L'0';
}

HRESULT createD3D12Device(IDXGIAdapter1* adapter, D3D_FEATURE_LEVEL level) {
    return D3D12CreateDevice(adapter, level, __uuidof(ID3D12Device), nullptr);
}

AdapterInfo readAdapter(IDXGIAdapter1* adapter) {
    AdapterInfo info{};

    DXGI_ADAPTER_DESC1 desc{};
    if (SUCCEEDED(adapter->GetDesc1(&desc))) {
        info.name = wideToUtf8(desc.Description);
        info.vendorId = desc.VendorId;
        info.deviceId = desc.DeviceId;
        info.software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    }

    if (!info.software) {
        info.fl12Result = createD3D12Device(adapter, D3D_FEATURE_LEVEL_12_0);
        info.fl11Result = createD3D12Device(adapter, D3D_FEATURE_LEVEL_11_0);
        info.supportsFl12 = SUCCEEDED(info.fl12Result);
        info.supportsFl11 = SUCCEEDED(info.fl11Result);
    }

    return info;
}

}

ProbeResult probeD3D12Support() {
    ProbeResult result{};
    result.allowFl11 = !envDisabled(L"VVP_ALLOW_D3D12_FL11");

    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    }

    result.factoryResult = hr;
    result.factoryCreated = SUCCEEDED(hr);
    if (!result.factoryCreated) {
        return result;
    }

    ComPtr<IDXGIFactory6> factory6;
    factory.As(&factory6);

    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory6) {
            hr = factory6->EnumAdapterByGpuPreference(
                index,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter));
        } else {
            hr = factory->EnumAdapters1(index, &adapter);
        }

        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            result.enumerationResult = hr;
            break;
        }

        AdapterInfo info = readAdapter(adapter.Get());
        if (info.supportsFl12) {
            result.hasSupportedAdapter = true;
        } else if (result.allowFl11 && info.supportsFl11) {
            result.hasSupportedAdapter = true;
            result.usingFl11 = true;
        }

        result.adapters.push_back(std::move(info));
    }

    return result;
}

std::string formatFailureReport(const ProbeResult& result) {
    std::ostringstream stream;

    stream << "D3D12 support check failed.\n";
    stream << "Factory: " << (result.factoryCreated ? "created" : "failed")
           << " result=" << hresultText(result.factoryResult) << "\n";
    stream << "FL11 experimental mode: " << (result.allowFl11 ? "enabled" : "disabled") << "\n";

    if (FAILED(result.enumerationResult)) {
        stream << "Enumeration result: " << hresultText(result.enumerationResult) << "\n";
    }

    if (result.adapters.empty()) {
        stream << "Adapters: none\n";
        return stream.str();
    }

    for (std::size_t i = 0; i < result.adapters.size(); ++i) {
        const AdapterInfo& adapter = result.adapters[i];
        stream << "Adapter " << i << ": " << adapter.name << "\n";
        stream << "  vendor=" << hexText(adapter.vendorId, 4)
               << " device=" << hexText(adapter.deviceId, 4)
               << " software=" << (adapter.software ? "yes" : "no") << "\n";
        stream << "  D3D12 FL12.0: " << (adapter.supportsFl12 ? "yes" : "no")
               << " result=" << hresultText(adapter.fl12Result) << "\n";
        stream << "  D3D12 FL11.0: " << (adapter.supportsFl11 ? "yes" : "no")
               << " result=" << hresultText(adapter.fl11Result) << "\n";
    }

    return stream.str();
}

}
