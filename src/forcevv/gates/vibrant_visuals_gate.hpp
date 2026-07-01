#pragma once

namespace forcevv::gates {

struct GatePatchConfig {
    bool hasSupportedD3D12Adapter{};
};

bool installVibrantVisualsGateHooks(const GatePatchConfig& config);
void removeVibrantVisualsGateHooks();

}
