#include "forcevv/render/d3d12_downlevel_retry.hpp"

#include "forcevv/log.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <MinHook.h>

#include <atomic>
#include <algorithm>
#include <string>

namespace forcevv::render {
namespace {

using D3D12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
using CheckFeatureSupportFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, D3D12_FEATURE, void*, UINT);

D3D12CreateDeviceFn g_originalD3D12CreateDevice{};
CheckFeatureSupportFn g_originalCheckFeatureSupport{};
void* g_target{};
void* g_checkFeatureSupportTarget{};
std::atomic_bool g_loggedRetry{false};
std::atomic_bool g_loggedFeatureLevelSpoof{false};

bool disabledByEnv() {
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(L"VVP_DISABLE_D3D12_DOWNLEVEL_RETRY", value, static_cast<DWORD>(std::size(value)));
    return length != 0 && value[0] != L'0';
}

HRESULT STDMETHODCALLTYPE hkCheckFeatureSupport(
    ID3D12Device* self,
    D3D12_FEATURE feature,
    void* featureSupportData,
    UINT featureSupportDataSize) {
    HRESULT result = g_originalCheckFeatureSupport(self, feature, featureSupportData, featureSupportDataSize);
    if (feature != D3D12_FEATURE_FEATURE_LEVELS
        || featureSupportData == nullptr
        || featureSupportDataSize < sizeof(D3D12_FEATURE_DATA_FEATURE_LEVELS)) {
        return result;
    }

    auto* levels = static_cast<D3D12_FEATURE_DATA_FEATURE_LEVELS*>(featureSupportData);
    D3D_FEATURE_LEVEL reported = D3D_FEATURE_LEVEL_12_0;
    for (UINT i = 0; i < levels->NumFeatureLevels; ++i) {
        reported = std::max(reported, levels->pFeatureLevelsRequested[i]);
    }
    if (reported > D3D_FEATURE_LEVEL_12_0) {
        reported = D3D_FEATURE_LEVEL_12_0;
    }

    levels->MaxSupportedFeatureLevel = reported;
    if (!g_loggedFeatureLevelSpoof.exchange(true)) {
        log::warn("D3D12 feature-level query spoofed to FL12.0.");
    }

    return S_OK;
}

bool installCheckFeatureSupportHook(IUnknown* createdDevice) {
    if (g_checkFeatureSupportTarget != nullptr || createdDevice == nullptr) {
        return true;
    }

    ID3D12Device* device{};
    if (FAILED(createdDevice->QueryInterface(__uuidof(ID3D12Device), reinterpret_cast<void**>(&device))) || device == nullptr) {
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(device);
    void* target = vtable[13];
    device->Release();

    MH_STATUS createStatus = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&hkCheckFeatureSupport),
        reinterpret_cast<void**>(&g_originalCheckFeatureSupport));
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        log::warn(std::string("Failed to create D3D12 feature-level spoof hook: ") + MH_StatusToString(createStatus));
        return false;
    }

    MH_STATUS enableStatus = MH_EnableHook(target);
    if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED) {
        log::warn(std::string("Failed to enable D3D12 feature-level spoof hook: ") + MH_StatusToString(enableStatus));
        return false;
    }

    g_checkFeatureSupportTarget = target;
    log::info("D3D12 feature-level spoof hook installed.");
    return true;
}

HRESULT WINAPI hkD3D12CreateDevice(
    IUnknown* adapter,
    D3D_FEATURE_LEVEL minimumFeatureLevel,
    REFIID riid,
    void** device) {
    HRESULT result = g_originalD3D12CreateDevice(adapter, minimumFeatureLevel, riid, device);
    if (SUCCEEDED(result) || minimumFeatureLevel <= D3D_FEATURE_LEVEL_11_1) {
        return result;
    }

    if (!g_loggedRetry.exchange(true)) {
        log::warn("D3D12 FL12.x device creation failed; retrying FL11.1/FL11.0.");
    }

    result = g_originalD3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_1, riid, device);
    if (SUCCEEDED(result)) {
        if (device != nullptr && *device != nullptr) {
            installCheckFeatureSupportHook(static_cast<IUnknown*>(*device));
        }
        return result;
    }

    result = g_originalD3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, riid, device);
    if (SUCCEEDED(result) && device != nullptr && *device != nullptr) {
        installCheckFeatureSupportHook(static_cast<IUnknown*>(*device));
    }
    return result;
}

}

bool installD3D12DownlevelRetry() {
    if (g_target != nullptr) {
        return true;
    }
    if (disabledByEnv()) {
        return true;
    }

    HMODULE module = GetModuleHandleW(L"d3d12.dll");
    if (module == nullptr) {
        module = LoadLibraryW(L"d3d12.dll");
    }
    if (module == nullptr) {
        log::warn("d3d12.dll was not available for downlevel retry hook.");
        return false;
    }

    void* target = reinterpret_cast<void*>(GetProcAddress(module, "D3D12CreateDevice"));
    if (target == nullptr) {
        log::warn("D3D12CreateDevice was not available for downlevel retry hook.");
        return false;
    }

    MH_STATUS createStatus = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&hkD3D12CreateDevice),
        reinterpret_cast<void**>(&g_originalD3D12CreateDevice));
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        log::warn(std::string("Failed to create D3D12 downlevel retry hook: ") + MH_StatusToString(createStatus));
        return false;
    }

    MH_STATUS enableStatus = MH_EnableHook(target);
    if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED) {
        log::warn(std::string("Failed to enable D3D12 downlevel retry hook: ") + MH_StatusToString(enableStatus));
        return false;
    }

    g_target = target;
    log::info("D3D12 downlevel retry hook installed.");
    return true;
}

void removeD3D12DownlevelRetry() {
    if (g_checkFeatureSupportTarget != nullptr) {
        MH_DisableHook(g_checkFeatureSupportTarget);
        g_checkFeatureSupportTarget = nullptr;
        g_originalCheckFeatureSupport = nullptr;
    }

    if (g_target == nullptr) {
        return;
    }

    MH_DisableHook(g_target);
    g_target = nullptr;
    g_originalD3D12CreateDevice = nullptr;
}

}
