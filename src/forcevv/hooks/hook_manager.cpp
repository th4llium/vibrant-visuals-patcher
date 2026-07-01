#include "forcevv/hooks/hook_manager.hpp"

#include "forcevv/log.hpp"

#include <MinHook.h>

#include <atomic>
#include <string>

namespace forcevv::hooks {
namespace {

std::atomic_bool g_initialized{false};

const char* statusToString(MH_STATUS status) {
    return MH_StatusToString(status);
}

}

bool initialize() {
    if (g_initialized.load()) {
        return true;
    }

    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        log::error(std::string("MinHook initialization failed: ") + statusToString(status));
        return false;
    }

    g_initialized.store(true);
    return true;
}

void shutdown() {
    if (!g_initialized.exchange(false)) {
        return;
    }

    const MH_STATUS disableStatus = MH_DisableHook(MH_ALL_HOOKS);
    if (disableStatus != MH_OK && disableStatus != MH_ERROR_NOT_INITIALIZED) {
        log::warn(std::string("MinHook disable-all returned: ") + statusToString(disableStatus));
    }

    const MH_STATUS uninitStatus = MH_Uninitialize();
    if (uninitStatus != MH_OK && uninitStatus != MH_ERROR_NOT_INITIALIZED) {
        log::warn(std::string("MinHook uninitialize returned: ") + statusToString(uninitStatus));
    }
}

bool isInitialized() {
    return g_initialized.load();
}

}
