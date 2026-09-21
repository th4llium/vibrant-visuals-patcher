#include "forcevv/dx/dx12_probe.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdio>
#include <string>

using Microsoft::WRL::ComPtr;

namespace forcevv::dx {
namespace {

void wideToUtf8(const wchar_t* src, char* dst, std::size_t dstSize) {
    if (src == nullptr || src[0] == L'\0' || dst == nullptr || dstSize == 0) {
        if (dst && dstSize > 0) dst[0] = '\0';
        return;
    }

    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, static_cast<int>(dstSize), nullptr, nullptr);
    dst[dstSize - 1] = '\0';
}

std::string hresultText(HRESULT result) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08X", static_cast<std::uint32_t>(result));
    return buf;
}

std::string hexText(std::uint32_t value, int width) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%0*X", width, value);
    return buf;
}

bool envDisabled(const wchar_t* name) {
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(name, value, static_cast<DWORD>(sizeof(value) / sizeof(wchar_t)));
    return length != 0 && value[0] == L'0';
}

typedef HRESULT (WINAPI *PFN_D3D12_CREATE_DEVICE)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
typedef HRESULT (WINAPI *PFN_CREATE_DXGI_FACTORY1)(REFIID, void**);
typedef HRESULT (WINAPI *PFN_CREATE_DXGI_FACTORY2)(UINT, REFIID, void**);

HRESULT createD3D12Device(IDXGIAdapter1* adapter, D3D_FEATURE_LEVEL level) {
    static HMODULE hD3D12 = LoadLibraryW(L"d3d12.dll");
    if (!hD3D12) {
        return E_FAIL;
    }
    static auto pfnCreateDevice = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
        GetProcAddress(hD3D12, "D3D12CreateDevice"));
    if (!pfnCreateDevice) {
        return E_FAIL;
    }
    return pfnCreateDevice(adapter, level, __uuidof(ID3D12Device), nullptr);
}

AdapterInfo readAdapter(IDXGIAdapter1* adapter) {
    AdapterInfo info{};

    DXGI_ADAPTER_DESC1 desc{};
    if (SUCCEEDED(adapter->GetDesc1(&desc))) {
        wideToUtf8(desc.Description, info.name, sizeof(info.name));
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

    HMODULE hDxgi = LoadLibraryW(L"dxgi.dll");
    if (!hDxgi) {
        result.factoryResult = HRESULT_FROM_WIN32(GetLastError());
        return result;
    }

    auto pfnCreateDXGIFactory2 = reinterpret_cast<PFN_CREATE_DXGI_FACTORY2>(
        GetProcAddress(hDxgi, "CreateDXGIFactory2"));
    auto pfnCreateDXGIFactory1 = reinterpret_cast<PFN_CREATE_DXGI_FACTORY1>(
        GetProcAddress(hDxgi, "CreateDXGIFactory1"));

    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = E_FAIL;
    if (pfnCreateDXGIFactory2) {
        hr = pfnCreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    }
    if (FAILED(hr) && pfnCreateDXGIFactory1) {
        hr = pfnCreateDXGIFactory1(IID_PPV_ARGS(&factory));
    }

    result.factoryResult = hr;
    result.factoryCreated = SUCCEEDED(hr);
    if (!result.factoryCreated) {
        return result;
    }

    ComPtr<IDXGIFactory6> factory6;
    factory.As(&factory6);

    for (UINT index = 0; index < MAX_ADAPTERS; ++index) {
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

        result.adapters[result.adapterCount++] = info;
    }

    return result;
}

std::string formatFailureReport(const ProbeResult& result) {
    std::string report = "D3D12 support check failed.\n";
    report += "Factory: ";
    report += (result.factoryCreated ? "created" : "failed");
    report += " result=" + hresultText(result.factoryResult) + "\n";
    report += "FL11 experimental mode: ";
    report += (result.allowFl11 ? "enabled\n" : "disabled\n");

    if (FAILED(result.enumerationResult)) {
        report += "Enumeration result: " + hresultText(result.enumerationResult) + "\n";
    }

    if (result.adapterCount == 0) {
        report += "Adapters: none\n";
        return report;
    }

    for (std::size_t i = 0; i < result.adapterCount; ++i) {
        const AdapterInfo& adapter = result.adapters[i];
        char numBuf[16];
        snprintf(numBuf, sizeof(numBuf), "%zu", i);
        report += "Adapter ";
        report += numBuf;
        report += ": ";
        report += adapter.name;
        report += "\n";
        report += "  vendor=" + hexText(adapter.vendorId, 4);
        report += " device=" + hexText(adapter.deviceId, 4);
        report += " software=" + std::string(adapter.software ? "yes\n" : "no\n");
        report += "  D3D12 FL12.0: " + std::string(adapter.supportsFl12 ? "yes" : "no");
        report += " result=" + hresultText(adapter.fl12Result) + "\n";
        report += "  D3D12 FL11.0: " + std::string(adapter.supportsFl11 ? "yes" : "no");
        report += " result=" + hresultText(adapter.fl11Result) + "\n";
    }

    return report;
}

}
