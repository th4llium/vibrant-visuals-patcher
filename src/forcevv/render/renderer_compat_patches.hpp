#pragma once

namespace forcevv::render {

void installRendererCompatibilityPatchesEarlyNoLog();
void installRendererCompatibilityPatchesEarlyForBrdNoLog();
bool installRendererCompatibilityPatches();
void removeRendererCompatibilityPatches();

}
