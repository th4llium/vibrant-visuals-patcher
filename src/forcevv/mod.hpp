#pragma once

#include <Windows.h>

namespace forcevv {

int run(HMODULE module);
void shutdown();
void onProcessDetach();

}
