#include "forcevv/gates/vibrant_visuals_gate.hpp"

#include "forcevv/log.hpp"

#include <Windows.h>
#include <MinHook.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace forcevv::gates {
namespace {

struct ModuleInfo {
    std::uint8_t* base{};
    std::size_t size{};
};

struct TextSection {
    std::uint8_t* begin{};
    std::size_t size{};
};

using CapabilityFlagsFn = bool(__fastcall*)(void*);

constexpr std::array<int, 18> kCapabilityFlagsSignature{
    0x80, 0x79, -1, -1,
    0x74, -1,
    0x31, 0xC0,
    0xC3,
    0x0F, 0xB6, 0x41, -1,
    0xC3,
    0xCC, 0xCC,
    0x88, 0x51,
};

constexpr std::uintptr_t kCapabilityFlags26_31Rva = 0x068A7280;

CapabilityFlagsFn g_originalCapabilityFlags{};
void* g_capabilityFlagsTarget{};
std::atomic_bool g_loggedCapabilityFlagsCall{false};

std::string formatPointer(const void* pointer) {
    char buffer[32]{};
    sprintf_s(buffer, "0x%p", pointer);
    return buffer;
}

std::string formatHex(std::uintptr_t value) {
    char buffer[32]{};
    sprintf_s(buffer, "0x%llX", static_cast<unsigned long long>(value));
    return buffer;
}

std::string formatFlagBytes(const std::uint8_t* flags) {
    std::string result = "[";
    for (std::size_t i = 0; i < 5; ++i) {
        if (i != 0) {
            result += ",";
        }
        result += std::to_string(static_cast<unsigned int>(flags[i]));
    }
    result += "]";
    return result;
}

ModuleInfo mainModuleInfo() {
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

std::uint8_t* addressFromRva(const ModuleInfo& module, std::uintptr_t rva, std::size_t requiredSize) {
    if (module.base == nullptr || rva > module.size || requiredSize > module.size - rva) {
        return nullptr;
    }

    return module.base + rva;
}

bool patternMatches(const std::uint8_t* cursor, const int* pattern, std::size_t size) {
    if (cursor == nullptr || pattern == nullptr) {
        return false;
    }

    for (std::size_t i = 0; i < size; ++i) {
        if (pattern[i] >= 0 && cursor[i] != static_cast<std::uint8_t>(pattern[i])) {
            return false;
        }
    }

    return true;
}

TextSection findTextSection(const ModuleInfo& module) {
    if (module.base == nullptr) {
        return {};
    }

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module.base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(module.base + dos->e_lfanew);
    auto* section = IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (std::memcmp(section->Name, ".text", 5) != 0) {
            continue;
        }

        const std::size_t virtualAddress = section->VirtualAddress;
        const std::size_t virtualSize = section->Misc.VirtualSize != 0
            ? section->Misc.VirtualSize
            : section->SizeOfRawData;
        if (virtualAddress > module.size || virtualSize > module.size - virtualAddress) {
            return {};
        }

        return {module.base + virtualAddress, virtualSize};
    }

    return {};
}

void* scanCapabilityFlagsSignature(const ModuleInfo& module) {
    const TextSection text = findTextSection(module);
    if (text.begin == nullptr || text.size < kCapabilityFlagsSignature.size()) {
        log::warn("Vibrant Visuals capability signature scan skipped because the .text section was unavailable.");
        return nullptr;
    }

    const std::size_t maxOffset = text.size - kCapabilityFlagsSignature.size();
    for (std::size_t offset = 0; offset <= maxOffset; ++offset) {
        auto* cursor = text.begin + offset;
        if (patternMatches(cursor, kCapabilityFlagsSignature.data(), kCapabilityFlagsSignature.size())) {
            log::info("Vibrant Visuals capability signature found.");
            return cursor;
        }
    }

    log::warn("Vibrant Visuals capability signature was not found in .text.");
    return nullptr;
}

void* resolveCapabilityFlagsTarget() {
    const ModuleInfo module = mainModuleInfo();
    if (module.base == nullptr) {
        log::error("Could not resolve the main executable module for the Vibrant Visuals capability hook.");
        return nullptr;
    }

    auto* fastTarget = addressFromRva(module, kCapabilityFlags26_31Rva, kCapabilityFlagsSignature.size());
    if (patternMatches(fastTarget, kCapabilityFlagsSignature.data(), kCapabilityFlagsSignature.size())) {
        log::info("Vibrant Visuals capability predicate matched.");
        return fastTarget;
    }

    log::warn("Vibrant Visuals capability predicate did not match the known 26.31 RVA; scanning for a moved function.");
    return scanCapabilityFlagsSignature(module);
}

bool __fastcall hkCapabilityFlags(void* self) {
    if (self != nullptr) {
        auto* flags = reinterpret_cast<std::uint8_t*>(self) + 48;
        if (!g_loggedCapabilityFlagsCall.exchange(true)) {
            log::info("Vibrant Visuals capability forced.");
        }

        flags[0] = 1;
        flags[1] = 1;
        flags[2] = 1;
        flags[3] = 0;
        flags[4] = 1;
    } else if (!g_loggedCapabilityFlagsCall.exchange(true)) {
        log::warn("Vibrant Visuals capability predicate was queried with a null object; forcing true only.");
    }

    return true;
}

}

bool installVibrantVisualsGateHooks(const GatePatchConfig& config) {
    if (!config.hasSupportedD3D12Adapter) {
        log::warn("Refusing to install the Vibrant Visuals capability hook.");
        return false;
    }

    if (g_capabilityFlagsTarget != nullptr) {
        return true;
    }

    void* target = resolveCapabilityFlagsTarget();
    if (target == nullptr) {
        log::warn("Vibrant Visuals capability hook was not installed.");
        return false;
    }

    const MH_STATUS createStatus = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&hkCapabilityFlags),
        reinterpret_cast<void**>(&g_originalCapabilityFlags));
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        log::warn(std::string("Failed to create Vibrant Visuals capability hook: ") + MH_StatusToString(createStatus));
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(target);
    if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED) {
        log::warn(std::string("Failed to enable Vibrant Visuals capability hook: ") + MH_StatusToString(enableStatus));
        return false;
    }

    g_capabilityFlagsTarget = target;
    log::info("Vibrant Visuals capability hook installed.");
    return true;
}

void removeVibrantVisualsGateHooks() {
    if (g_capabilityFlagsTarget == nullptr) {
        return;
    }

    MH_DisableHook(g_capabilityFlagsTarget);
    g_capabilityFlagsTarget = nullptr;
    g_originalCapabilityFlags = nullptr;
}

}
