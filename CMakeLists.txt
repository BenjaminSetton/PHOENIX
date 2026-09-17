cmake_minimum_required(VERSION 3.24)

project(PHOENIX CXX)

option(PHX_BUILD_SAMPLES "Build PHOENIX sample apps" ${PROJECT_IS_TOP_LEVEL})

# ---------------------------------------------------------------------------
# Configurations
# ---------------------------------------------------------------------------
# Only defined when PHOENIX is the top-level project. Consumers embedding via
# add_subdirectory are expected to define the Debug;Release;Sanitizer configs
# (and matching Sanitizer flags / runtime library) themselves.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Sanitizer" CACHE STRING "" FORCE)

    # Sanitizer-specific compiler/linker flags
    set(CMAKE_CXX_FLAGS_SANITIZER "/Zi /Od /fsanitize=address" CACHE STRING "Sanitizer C++ flags" FORCE)
    set(CMAKE_C_FLAGS_SANITIZER "/Zi /Od /fsanitize=address" CACHE STRING "Sanitizer C flags" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_SANITIZER "/INCREMENTAL:NO" CACHE STRING "Sanitizer exe linker flags" FORCE)

    # MSVC runtime: Debug and Sanitizer use debug runtime, Release uses release
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Sanitizer>:Debug>DLL")
endif()

# ---------------------------------------------------------------------------
# Architecture
# ---------------------------------------------------------------------------
# Use CMake's platform naming directly (x64, ARM64, etc.) for output dirs.
# For single-config generators (Ninja, Makefile), fall back to CMAKE_SYSTEM_PROCESSOR
if(DEFINED CMAKE_GENERATOR_PLATFORM)
    set(PHX_ARCH_DIR "${CMAKE_GENERATOR_PLATFORM}")
else()
    set(PHX_ARCH_DIR "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(PHX_ROOT     "${CMAKE_CURRENT_SOURCE_DIR}/src/PHOENIX")
set(SAMPLES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/samples")
set(BSL_ROOT     "${CMAKE_CURRENT_SOURCE_DIR}/src/BSL")

# Prebuilt dependencies (GLFW, Slang) must exist before configuring
if(NOT EXISTS "${PHX_ROOT}/out")
    message(FATAL_ERROR
        "Prebuilt dependencies not found at ${PHX_ROOT}/out. "
        "Run utils/build_lib_dependencies.ps1 first to build GLFW and Slang.")
endif()

# System name in lowercase (windows, linux, darwin)
string(TOLOWER "${CMAKE_SYSTEM_NAME}" PHX_SYSTEM_DIR)

# Export path segments to consumers so phx_set_output_dirs() works from their scope
if(NOT PROJECT_IS_TOP_LEVEL)
    set(PHX_ARCH_DIR   "${PHX_ARCH_DIR}"   PARENT_SCOPE)
    set(PHX_SYSTEM_DIR "${PHX_SYSTEM_DIR}" PARENT_SCOPE)
endif()

# ---------------------------------------------------------------------------
# Vulkan SDK
# ---------------------------------------------------------------------------
set(VULKAN_SDK "$ENV{VULKAN_SDK}")
if(NOT VULKAN_SDK OR VULKAN_SDK STREQUAL "")
    message(FATAL_ERROR "VULKAN_SDK environment variable is not set. Vulkan is likely not installed or misconfigured!")
endif()

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable solution folders in VS (groups targets in Solution Explorer)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Enable multi-processor compilation (/MP) for MSVC
if(MSVC)
    add_compile_options(/MP)
endif()

# Default startup project for VS
if(PHX_BUILD_SAMPLES)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "ImGui")
endif()

# ---------------------------------------------------------------------------
# Output directory helper
# ---------------------------------------------------------------------------
# Sets per-config ARCHIVE/RUNTIME/LIBRARY output directories for a target
function(phx_set_output_dirs target base_dir)
    foreach(cfg Debug Release Sanitizer)
        string(TOLOWER ${cfg} cfgLower)
        set(dir "${base_dir}/${PHX_SYSTEM_DIR}/${cfgLower}/${PHX_ARCH_DIR}")
        # Per-config output directory properties use uppercase <CONFIG> suffix
        string(TOUPPER ${cfg} cfgUpper)
        set_target_properties(${target} PROPERTIES
            "ARCHIVE_OUTPUT_DIRECTORY_${cfgUpper}"  "${dir}"
            "RUNTIME_OUTPUT_DIRECTORY_${cfgUpper}"  "${dir}"
            "LIBRARY_OUTPUT_DIRECTORY_${cfgUpper}"  "${dir}"
        )
    endforeach()
endfunction()

# ---------------------------------------------------------------------------
# Prebuilt dependency targets (imported)
# ---------------------------------------------------------------------------
# Vulkan
add_library(phx_vulkan SHARED IMPORTED)
set_target_properties(phx_vulkan PROPERTIES
    IMPORTED_LOCATION "${VULKAN_SDK}/Bin/vulkan-1.dll"
    IMPORTED_IMPLIB   "${VULKAN_SDK}/Lib/vulkan-1.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${PHX_ROOT}/vendor/vulkan/include"
)

# GLFW
add_library(phx_glfw STATIC IMPORTED)
set_target_properties(phx_glfw PROPERTIES
    IMPORTED_LOCATION_DEBUG     "${PHX_ROOT}/out/glfw/bin/${PHX_SYSTEM_DIR}/debug/${PHX_ARCH_DIR}/glfw3d.lib"
    IMPORTED_LOCATION_RELEASE   "${PHX_ROOT}/out/glfw/bin/${PHX_SYSTEM_DIR}/release/${PHX_ARCH_DIR}/glfw3.lib"
    IMPORTED_LOCATION_SANITIZER "${PHX_ROOT}/out/glfw/bin/${PHX_SYSTEM_DIR}/sanitizer/${PHX_ARCH_DIR}/glfw3d.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${PHX_ROOT}/vendor/glfw/include"
)

# Slang - sanitizer links against debug
add_library(phx_slang STATIC IMPORTED)
set_target_properties(phx_slang PROPERTIES
    IMPORTED_LOCATION_DEBUG     "${PHX_ROOT}/out/slang/bin/${PHX_SYSTEM_DIR}/debug/${PHX_ARCH_DIR}/slang-compilerd.lib"
    IMPORTED_LOCATION_RELEASE   "${PHX_ROOT}/out/slang/bin/${PHX_SYSTEM_DIR}/release/${PHX_ARCH_DIR}/slang-compiler.lib"
    IMPORTED_LOCATION_SANITIZER "${PHX_ROOT}/out/slang/bin/${PHX_SYSTEM_DIR}/debug/${PHX_ARCH_DIR}/slang-compilerd.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${PHX_ROOT}/vendor/slang/include"
)

# VMA — header-only
add_library(phx_vma INTERFACE IMPORTED)
set_target_properties(phx_vma PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PHX_ROOT}/vendor/vma/include"
)

# BSL — header-only
add_library(bsl INTERFACE IMPORTED)
set_target_properties(bsl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BSL_ROOT}"
)

# ---------------------------------------------------------------------------
# Sub-projects
# ---------------------------------------------------------------------------
add_subdirectory(src/PHOENIX)
if(PHX_BUILD_SAMPLES)
    add_subdirectory(src/samples)
endif()

# ---------------------------------------------------------------------------
# VS solution organization
# ---------------------------------------------------------------------------
# Hide CMake utility targets (ALL_BUILD, ZERO_CHECK) in a separate folder
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
