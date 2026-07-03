#include "forcevv/render/renderer_compat_patches.hpp"

#include "forcevv/log.hpp"

#include <Windows.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace forcevv::render {
namespace {

struct ModuleInfo {
    std::uint8_t* base{};
    std::size_t size{};
};

struct BytePatch {
    std::uint8_t* address{};
    std::vector<std::uint8_t> original;
};

BytePatch g_vendorGatePatch{};
BytePatch g_samplerFlagsPatch{};
std::uint8_t* g_earlyVendorGatePatch{};
std::uint8_t* g_earlySamplerFlagsPatch{};

constexpr std::uintptr_t kD3D12VendorGate26_31Rva = 0x0D30068F;
constexpr std::uintptr_t kSamplerFlags26_31Rva = 0x0B769D36;

constexpr std::array<int, 12> kD3D12VendorGateLongSignature{
    0x81, 0xBE, -1, -1, -1, -1,
    0x86, 0x80, 0x00, 0x00,
    0x0F, 0x85,
};

constexpr std::array<int, 11> kD3D12VendorGateShortSignature{
    0x81, 0xBE, -1, -1, -1, -1,
    0x86, 0x80, 0x00, 0x00,
    0x75,
};

constexpr std::array<int, 12> kSamplerFlagsSignature{
    0x0D, -1, -1, -1, -1,
    0x80, 0x79, -1, -1,
    0x0F, 0x44, 0xC2,
};

ModuleInfo getMainModuleInfo() {
    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (base == nullptr) {
        return {};
    }

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return {};
    }

    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return {};
    }

    return {base, nt->OptionalHeader.SizeOfImage};
}

std::uint8_t* addressFromRva(std::uintptr_t rva, std::size_t requiredSize) {
    const ModuleInfo module = getMainModuleInfo();
    if (module.base == nullptr || rva > module.size || requiredSize > module.size - rva) {
        return nullptr;
    }

    return module.base + rva;
}

bool isD3D12VendorGateShape(const std::uint8_t* ptr) {
    return ptr != nullptr
        && ptr[0] == 0x81
        && ptr[1] == 0xBE
        && ptr[8] == 0x00
        && ptr[9] == 0x00
        && (ptr[10] == 0x75 || (ptr[10] == 0x0F && ptr[11] == 0x85));
}

bool isD3D12VendorGateUnpatched(const std::uint8_t* ptr) {
    return isD3D12VendorGateShape(ptr)
        && ptr[6] == 0x86
        && ptr[7] == 0x80;
}

bool isD3D12VendorGatePatched(const std::uint8_t* ptr) {
    return isD3D12VendorGateShape(ptr)
        && ptr[6] == 0x00
        && ptr[7] == 0x00;
}

bool isSamplerFlagsShape(const std::uint8_t* ptr) {
    return ptr != nullptr
        && ptr[0] == 0x0D
        && ptr[5] == 0x80
        && ptr[6] == 0x79
        && ptr[9] == 0x0F
        && ptr[10] == 0x44
        && ptr[11] == 0xC2;
}

void writeEarlyBytes(std::uint8_t* address, const std::uint8_t* bytes, std::size_t size) {
    if (address == nullptr || bytes == nullptr || size == 0) {
        return;
    }

    DWORD oldProtect{};
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return;
    }

    std::memcpy(address, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), address, size);
    DWORD ignored{};
    VirtualProtect(address, size, oldProtect, &ignored);
}

template <std::size_t Size>
bool patternMatches(const std::uint8_t* cursor, const std::array<int, Size>& pattern) {
    if (cursor == nullptr) {
        return false;
    }

    for (std::size_t i = 0; i < Size; ++i) {
        if (pattern[i] >= 0 && cursor[i] != static_cast<std::uint8_t>(pattern[i])) {
            return false;
        }
    }

    return true;
}

template <std::size_t Size>
std::uint8_t* findPattern(const std::array<int, Size>& pattern) {
    const ModuleInfo module = getMainModuleInfo();
    if (module.base == nullptr || module.size < Size) {
        return nullptr;
    }

    const std::size_t maxOffset = module.size - Size;
    for (std::size_t offset = 0; offset <= maxOffset; ++offset) {
        auto* cursor = module.base + offset;
        if (patternMatches(cursor, pattern)) {
            return cursor;
        }
    }

    return nullptr;
}

bool writeBytes(std::uint8_t* address, const std::vector<std::uint8_t>& bytes, BytePatch& patch, std::string_view label) {
    if (address == nullptr || bytes.empty()) {
        return false;
    }

    DWORD oldProtect{};
    if (!VirtualProtect(address, bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        log::warn(std::string(label) + " could not change page protection.");
        return false;
    }

    patch.address = address;
    patch.original.assign(address, address + bytes.size());
    std::memcpy(address, bytes.data(), bytes.size());

    FlushInstructionCache(GetCurrentProcess(), address, bytes.size());
    DWORD ignored{};
    VirtualProtect(address, bytes.size(), oldProtect, &ignored);
    return true;
}

bool restorePatch(BytePatch& patch, std::string_view label) {
    if (patch.address == nullptr || patch.original.empty()) {
        return false;
    }

    DWORD oldProtect{};
    if (!VirtualProtect(patch.address, patch.original.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        log::warn(std::string(label) + " restore could not change page protection.");
        return false;
    }

    std::memcpy(patch.address, patch.original.data(), patch.original.size());
    FlushInstructionCache(GetCurrentProcess(), patch.address, patch.original.size());
    DWORD ignored{};
    VirtualProtect(patch.address, patch.original.size(), oldProtect, &ignored);
    patch = {};
    return true;
}

std::uint8_t* findMovedVendorGate() {
    auto* ptr = findPattern(kD3D12VendorGateLongSignature);
    return ptr != nullptr ? ptr : findPattern(kD3D12VendorGateShortSignature);
}

std::uint8_t* findMovedSamplerFlags() {
    return findPattern(kSamplerFlagsSignature);
}

bool patchVendorGate() {
    if (g_vendorGatePatch.address != nullptr) {
        return true;
    }

    if (isD3D12VendorGatePatched(g_earlyVendorGatePatch)) {
        log::info("D3D12 vendor gate patched early.");
        return true;
    }

    auto* ptr = addressFromRva(kD3D12VendorGate26_31Rva, 15);
    if (isD3D12VendorGatePatched(ptr)) {
        log::info("D3D12 vendor gate patched early.");
        return true;
    }

    if (isD3D12VendorGateUnpatched(ptr)) {
        log::info("D3D12 vendor gate matched.");
    } else {
        if (ptr != nullptr) {
            log::warn("D3D12 vendor gate fast path did not match.");
        }
        ptr = findMovedVendorGate();
    }

    if (ptr == nullptr) {
        log::warn("D3D12 vendor gate patch did not find RendererContextD3D12::init vendor check.");
        return false;
    }

    const std::vector<std::uint8_t> replacement{0x00, 0x00};
    if (!writeBytes(ptr + 6, replacement, g_vendorGatePatch, "D3D12 vendor gate patch")) {
        return false;
    }

    log::info("D3D12 vendor gate patched.");
    return true;
}

bool patchSamplerFlags() {
    if (g_samplerFlagsPatch.address != nullptr) {
        return true;
    }

    if (isSamplerFlagsShape(g_earlySamplerFlagsPatch) && g_earlySamplerFlagsPatch[4] == 0x00) {
        log::info("D3D12 sampler-flags patched early.");
        return true;
    }

    auto* ptr = addressFromRva(kSamplerFlags26_31Rva, 13);
    if (isSamplerFlagsShape(ptr) && ptr[4] == 0x00) {
        log::info("D3D12 sampler-flags patched early.");
        return true;
    }

    if (isSamplerFlagsShape(ptr)) {
        log::info("D3D12 sampler-flags matched.");
    } else {
        if (ptr != nullptr) {
            log::warn("D3D12 sampler-flags fast path did not match 26.31 bytes.");
        }
        ptr = findMovedSamplerFlags();
    }

    if (ptr == nullptr) {
        log::warn("D3D12 sampler-flags patch did not find dragon::bgfximpl::toSamplerFlags.");
        return false;
    }

    const std::vector<std::uint8_t> replacement{0x00};
    if (!writeBytes(ptr + 4, replacement, g_samplerFlagsPatch, "D3D12 sampler-flags patch")) {
        return false;
    }

    log::info("D3D12 sampler-flags patched.");
    return true;
}

}

void installRendererCompatibilityPatchesEarlyNoLog() {
    auto* vendor = addressFromRva(kD3D12VendorGate26_31Rva, 15);
    if (!isD3D12VendorGateUnpatched(vendor)) {
        vendor = findMovedVendorGate();
    }
    if (isD3D12VendorGateUnpatched(vendor)) {
        constexpr std::uint8_t replacement[]{0x00, 0x00};
        writeEarlyBytes(vendor + 6, replacement, sizeof(replacement));
        g_earlyVendorGatePatch = vendor;
    }

    auto* sampler = addressFromRva(kSamplerFlags26_31Rva, 13);
    if (!(isSamplerFlagsShape(sampler) && sampler[4] != 0x00)) {
        sampler = findMovedSamplerFlags();
    }
    if (isSamplerFlagsShape(sampler) && sampler[4] != 0x00) {
        constexpr std::uint8_t replacement[]{0x00};
        writeEarlyBytes(sampler + 4, replacement, sizeof(replacement));
        g_earlySamplerFlagsPatch = sampler;
    }
}

void installRendererCompatibilityPatchesEarlyForBrdNoLog() {
    auto* vendor = addressFromRva(kD3D12VendorGate26_31Rva, 16);
    if (!isD3D12VendorGateUnpatched(vendor)) {
        vendor = findMovedVendorGate();
    }
    if (isD3D12VendorGateUnpatched(vendor)) {
        if (vendor[10] == 0x0F && vendor[11] == 0x85) {
            std::int32_t relative{};
            std::memcpy(&relative, vendor + 12, sizeof(relative));
            ++relative;

            std::uint8_t replacement[6]{0xE9, 0, 0, 0, 0, 0x90};
            std::memcpy(replacement + 1, &relative, sizeof(relative));
            writeEarlyBytes(vendor + 10, replacement, sizeof(replacement));
            g_earlyVendorGatePatch = vendor;
        } else if (vendor[10] == 0x75) {
            constexpr std::uint8_t replacement[]{0xEB};
            writeEarlyBytes(vendor + 10, replacement, sizeof(replacement));
            g_earlyVendorGatePatch = vendor;
        }
    }

    auto* sampler = addressFromRva(kSamplerFlags26_31Rva, 13);
    if (!(isSamplerFlagsShape(sampler) && sampler[4] != 0x00)) {
        sampler = findMovedSamplerFlags();
    }
    if (isSamplerFlagsShape(sampler) && sampler[4] != 0x00) {
        constexpr std::uint8_t replacement[]{0x00};
        writeEarlyBytes(sampler + 4, replacement, sizeof(replacement));
        g_earlySamplerFlagsPatch = sampler;
    }
}

bool installRendererCompatibilityPatches() {
    bool installedAny = false;

    installedAny = patchVendorGate() || installedAny;
    installedAny = patchSamplerFlags() || installedAny;

    if (!installedAny) {
        log::warn("No renderer compatibility patches were installed.");
    }

    return installedAny;
}

void removeRendererCompatibilityPatches() {
    restorePatch(g_samplerFlagsPatch, "D3D12 sampler-flags patch");
    restorePatch(g_vendorGatePatch, "D3D12 vendor gate patch");
}

}
