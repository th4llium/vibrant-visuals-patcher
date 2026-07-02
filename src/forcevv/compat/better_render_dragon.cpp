#include "forcevv/compat/better_render_dragon.hpp"

#include <Windows.h>

namespace forcevv::compat {

bool betterRenderDragonLoaded() {
    return GetModuleHandleW(L"BetterRenderDragon.dll") != nullptr
        || GetModuleHandleW(L"BetterRenderDragon-no-imgui.dll") != nullptr;
}

}
