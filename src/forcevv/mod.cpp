#include "forcevv/mod.hpp"

#include "forcevv/compat/better_render_dragon.hpp"
#include "forcevv/dx/dx12_probe.hpp"
#include "forcevv/gates/vibrant_visuals_gate.hpp"
#include "forcevv/hooks/hook_manager.hpp"
#include "forcevv/log.hpp"
#include "forcevv/render/d3d12_downlevel_retry.hpp"
#include "forcevv/render/renderer_compat_patches.hpp"

#include <Windows.h>

#include <atomic>

namespace forcevv {
namespace {

std::atomic_bool g_started{false};
std::atomic_bool g_shutdown{false};

}

int run(HMODULE module) {
    (void)module;

    if (g_started.exchange(true)) {
        return 0;
    }

    log::initialize();
    log::info(VVP_NAME " v" VVP_VERSION);
    log::info("Author: " VVP_AUTHOR);

    if (!hooks::initialize()) {
        log::error("MinHook initialization failed.");
        return 1;
    }

    const bool betterRenderDragonLoaded = compat::betterRenderDragonLoaded();

    if (betterRenderDragonLoaded) {
        log::info("BetterRenderDragon detected; using BRD-compatible early renderer patch.");
    } else if (!render::installRendererCompatibilityPatches()) {
        log::warn("Renderer compatibility patch was not applied.");
    }

    const dx::ProbeResult probe = dx::probeD3D12Support();
    if (!probe.hasSupportedAdapter) {
        log::writeMultiline(log::Level::Error, dx::formatFailureReport(probe));
    } else if (probe.usingFl11) {
        log::warn("Using experimental D3D12 FL11.0 mode.");
    }
    if (!render::installD3D12DownlevelRetry()) {
        log::warn("D3D12 downlevel retry hook was not installed.");
    }

    gates::GatePatchConfig gateConfig{};
    gateConfig.hasSupportedD3D12Adapter = probe.hasSupportedAdapter;

    if (!gates::installVibrantVisualsGateHooks(gateConfig)) {
        log::warn("Vibrant Visuals capability hook was not installed.");
    }

    log::info("Startup complete.");
    return 0;
}

void shutdown() {
    if (g_shutdown.exchange(true)) {
        return;
    }

    gates::removeVibrantVisualsGateHooks();
    render::removeD3D12DownlevelRetry();
    render::removeRendererCompatibilityPatches();
    hooks::shutdown();
    log::shutdown();
}

void onProcessDetach() {
}

}
