#include "forcevv/mod.hpp"
#include "forcevv/compat/better_render_dragon.hpp"
#include "forcevv/render/renderer_compat_patches.hpp"

#include <Windows.h>

namespace {

DWORD WINAPI startupThread(LPVOID parameter) {
    forcevv::run(static_cast<HMODULE>(parameter));
    return 0;
}

}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;

    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(instance);
        if (forcevv::compat::betterRenderDragonLoaded()) {
            forcevv::render::installRendererCompatibilityPatchesEarlyForBrdNoLog();
        } else {
            forcevv::render::installRendererCompatibilityPatchesEarlyNoLog();
        }

        HANDLE thread = CreateThread(nullptr, 0, startupThread, instance, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
        break;
    }

    case DLL_PROCESS_DETACH:
        forcevv::onProcessDetach();
        break;

    default:
        break;
    }

    return TRUE;
}
