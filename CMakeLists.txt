cmake_minimum_required(VERSION 3.24)

project(PHOENIX CXX)

option(PHX_BUILD_SAMPLES "Build PHOENIX sample apps" OFF)

# ---------------------------------------------------------------------------
# Configurations
# ---------------------------------------------------------------------------
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
    message(FATAL_ERROR "Prebuilt dependencies not found at ${PHX_ROOT}/out. Run \"utils/build_lib_dependencies.ps1\" first to build them.")
endif()

# System name in lowercase (e.g. windows, linux)
string(TOLOWER "${CMAKE_SYSTEM_NAME}" PHX_SYSTEM_DIR)

# ---------------------------------------------------------------------------
# Config classification
# ---------------------------------------------------------------------------
# PHOENIX keys its prebuilt deps, defines and runtime settings on three config
# kinds: debug, sanitizer and profile; every other config is treated as
# Release. Consumers embedding via add_subdirectory declare which of their
# configs are which through these lists (defaults match PHOENIX standalone).
if(NOT DEFINED PHX_DEBUG_CONFIGS)
    set(PHX_DEBUG_CONFIGS Debug)
endif()
if(NOT DEFINED PHX_SANITIZER_CONFIGS)
    set(PHX_SANITIZER_CONFIGS Sanitizer)
endif()
if(NOT DEFINED PHX_PROFILE_CONFIGS)
    set(PHX_PROFILE_CONFIGS Release)
endif()

# $<CONFIG> conditions for each kind ("0" when the list is empty). Sanitizer
# counts as a debug build; Release is whatever isn't debug/sanitizer.
foreach(kind DEBUG SANITIZER PROFILE)
    if(PHX_${kind}_CONFIGS)
        set(PHX_IS_${kind} "$<IN_LIST:$<CONFIG>,${PHX_${kind}_CONFIGS}>")
    else()
        set(PHX_IS_${kind} "0")
    endif()
endforeach()
set(PHX_IS_DEBUG   "$<OR:${PHX_IS_DEBUG},${PHX_IS_SANITIZER}>")
set(PHX_IS_RELEASE "$<NOT:${PHX_IS_DEBUG}>")

# Map each build config onto the prebuilt deps' configs (Debug/Release/
# Sanitizer) so the matching imported binary gets picked. Done per-target via
# phx_map_imported_configs() below, since config-agnostic imported targets
# (e.g. phx_vulkan) must not get a mapping.

# Export paths and config conditions to consumers so phx_set_output_dirs()/
# phx_copy_slang_dlls() work from their scope
if(NOT PROJECT_IS_TOP_LEVEL)
    set(PHX_ROOT         "${PHX_ROOT}"         PARENT_SCOPE)
    set(PHX_ARCH_DIR     "${PHX_ARCH_DIR}"     PARENT_SCOPE)
    set(PHX_SYSTEM_DIR   "${PHX_SYSTEM_DIR}"   PARENT_SCOPE)
    set(PHX_IS_DEBUG     "${PHX_IS_DEBUG}"     PARENT_SCOPE)
    set(PHX_IS_RELEASE    "${PHX_IS_RELEASE}"    PARENT_SCOPE)
    set(PHX_IS_SANITIZER "${PHX_IS_SANITIZER}" PARENT_SCOPE)
    set(PHX_IS_PROFILE   "${PHX_IS_PROFILE}"   PARENT_SCOPE)
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

# Set the startup project
if(PROJECT_IS_TOP_LEVEL)
    if(PHX_BUILD_SAMPLES)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "ImGui")
    else()
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "PHOENIX")
    endif()
endif()

# ---------------------------------------------------------------------------
# Output directory helper
# ---------------------------------------------------------------------------
# Sets per-config ARCHIVE/RUNTIME/LIBRARY output directories for a target.
# Using a $<CONFIG> genex also stops multi-config generators from appending
# their own per-config subdirectory
function(phx_set_output_dirs target base_dir)
    set_target_properties(${target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${base_dir}/${PHX_SYSTEM_DIR}/$<LOWER_CASE:$<CONFIG>>/${PHX_ARCH_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${base_dir}/${PHX_SYSTEM_DIR}/$<LOWER_CASE:$<CONFIG>>/${PHX_ARCH_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${base_dir}/${PHX_SYSTEM_DIR}/$<LOWER_CASE:$<CONFIG>>/${PHX_ARCH_DIR}"
    )
endfunction()

# Points each build config of an imported target at the matching prebuilt
# binary config (Debug/Release/Sanitizer), per the classification above
function(phx_map_imported_configs target)
    foreach(cfg IN LISTS CMAKE_CONFIGURATION_TYPES CMAKE_BUILD_TYPE)
        if(cfg IN_LIST PHX_SANITIZER_CONFIGS)
            set(mapped Sanitizer)
        elseif(cfg IN_LIST PHX_DEBUG_CONFIGS)
            set(mapped Debug)
        else()
            set(mapped Release)
        endif()
        string(TOUPPER ${cfg} cfg_upper)
        set_property(TARGET ${target} PROPERTY MAP_IMPORTED_CONFIG_${cfg_upper} ${mapped})
    endforeach()
endfunction()

# Copies the Slang runtime DLLs next to a target's output after build. The
# config passed to the script is the PHOENIX config the build config maps to,
# so consumers with their own config names get the right (d-suffixed) DLLs
function(phx_copy_slang_dlls target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/copy_slang_dll.bat"
                "$<TARGET_FILE_DIR:${target}>"
                "$<IF:${PHX_IS_DEBUG},Debug,Release>"
                "${PHX_ROOT}/out/slang/bin/${PHX_SYSTEM_DIR}/$<IF:${PHX_IS_RELEASE},release,debug>/${PHX_ARCH_DIR}"
        VERBATIM
    )
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
phx_map_imported_configs(phx_glfw)

# Slang - sanitizer links against debug
add_library(phx_slang STATIC IMPORTED)
set_target_properties(phx_slang PROPERTIES
    IMPORTED_LOCATION_DEBUG     "${PHX_ROOT}/out/slang/bin/${PHX_SYSTEM_DIR}/debug/${PHX_ARCH_DIR}/slang-compilerd.lib"
    IMPORTED_LOCATION_RELEASE   "${PHX_ROOT}/out/slang/bin/${PHX_SYSTEM_DIR}/release/${PHX_ARCH_DIR}/slang-compiler.lib"
    IMPORTED_LOCATION_SANITIZER "${PHX_ROOT}/out/slang/bin/${PHX_SYSTEM_DIR}/debug/${PHX_ARCH_DIR}/slang-compilerd.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${PHX_ROOT}/vendor/slang/include"
)
phx_map_imported_configs(phx_slang)

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
