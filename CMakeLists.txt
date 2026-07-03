cmake_minimum_required(VERSION 3.24)

project(vibrant_visuals_patcher
    VERSION 1.0.2
    DESCRIPTION "Minecraft Bedrock Vibrant Visuals compatibility patcher"
    LANGUAGES CXX)

option(VVP_FETCH_DEPS "Fetch third-party dependencies with CMake FetchContent" ON)
option(VVP_ENABLE_CONSOLE "Open a debug console when the DLL is injected" OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DLL>")
endif()

add_library(vibrant_visuals_patcher SHARED
    src/dllmain.cpp
    src/forcevv/compat/better_render_dragon.cpp
    src/forcevv/dx/dx12_probe.cpp
    src/forcevv/gates/vibrant_visuals_gate.cpp
    src/forcevv/hooks/hook_manager.cpp
    src/forcevv/log.cpp
    src/forcevv/mod.cpp
    src/forcevv/render/d3d12_downlevel_retry.cpp
    src/forcevv/render/renderer_compat_patches.cpp)

set_target_properties(vibrant_visuals_patcher PROPERTIES
    OUTPUT_NAME "vibrant-visuals-patcher")

target_compile_features(vibrant_visuals_patcher PRIVATE cxx_std_20)

target_include_directories(vibrant_visuals_patcher PRIVATE
    src)

target_compile_definitions(vibrant_visuals_patcher PRIVATE
    VVP_NAME="vibrant-visuals-patcher"
    VVP_AUTHOR="th4llium"
    VVP_VERSION="${PROJECT_VERSION}"
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE)

if(VVP_ENABLE_CONSOLE)
    target_compile_definitions(vibrant_visuals_patcher PRIVATE VVP_ENABLE_CONSOLE=1)
endif()

if(MSVC)
    target_compile_options(vibrant_visuals_patcher PRIVATE
        /W4
        /permissive-
        /EHsc
        /Zc:__cplusplus)

else()
    target_compile_options(vibrant_visuals_patcher PRIVATE
        -Wall
        -Wextra
        -Wpedantic)
endif()

target_link_libraries(vibrant_visuals_patcher PRIVATE
    d3d12
    dxgi
    dxguid)

if(VVP_FETCH_DEPS)
    include(FetchContent)

    FetchContent_Declare(minhook
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG v1.3.4
        GIT_SHALLOW TRUE)

    FetchContent_MakeAvailable(minhook)

endif()

if(TARGET minhook)
    target_link_libraries(vibrant_visuals_patcher PRIVATE minhook)
elseif(TARGET MinHook::MinHook)
    target_link_libraries(vibrant_visuals_patcher PRIVATE MinHook::MinHook)
else()
    message(FATAL_ERROR "MinHook target was not found. Keep VVP_FETCH_DEPS=ON or provide a compatible MinHook target.")
endif()
