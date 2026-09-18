cmake_minimum_required(VERSION 3.24)

project(PHOENIX CXX)

option(PHX_BUILD_SAMPLES "Build PHOENIX sample apps" OFF)

# ---------------------------------------------------------------------------
# Configurations
# ---------------------------------------------------------------------------
# Only defined when PHOENIX is the top-level project. Consumers embedding via
# add_subdirectory use their own configs and map them onto PHOENIX's via
# PHX_CONFIG_MAP_<Config> (see Config mapping below).
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
# Config mapping
# ---------------------------------------------------------------------------
# PHOENIX's prebuilt deps and per-config defines are keyed on Debug/Release/
# Sanitizer; every config of the enclosing build must map to one of them.
# Same-named configs map to themselves; anything else requires the consumer to
# set PHX_CONFIG_MAP_<Config> (e.g. set(PHX_CONFIG_MAP_RelWithDebInfo Release))
# before add_subdirectory.
set(PHX_CONFIGS Debug Release Sanitizer)

if(CMAKE_CONFIGURATION_TYPES)
    set(PHX_BUILD_CONFIGS ${CMAKE_CONFIGURATION_TYPES})
elseif(CMAKE_BUILD_TYPE)
    set(PHX_BUILD_CONFIGS ${CMAKE_BUILD_TYPE})
else()
    message(FATAL_ERROR "CMAKE_BUILD_TYPE must be set when using a single-config generator")
endif()

# PHX_<PHXCFG>_CONFIGS = list of build configs that map to that PHOENIX config
foreach(phx_cfg IN LISTS PHX_CONFIGS)
    string(TOUPPER ${phx_cfg} phx_cfg_upper)
    set(PHX_${phx_cfg_upper}_CONFIGS "")
endforeach()

foreach(cfg IN LISTS PHX_BUILD_CONFIGS)
    if(cfg IN_LIST PHX_CONFIGS)
        set(mapped ${cfg})
    elseif(DEFINED PHX_CONFIG_MAP_${cfg})
        set(mapped ${PHX_CONFIG_MAP_${cfg}})
        if(NOT mapped IN_LIST PHX_CONFIGS)
            message(FATAL_ERROR "PHX_CONFIG_MAP_${cfg} is '${mapped}', must be one of: ${PHX_CONFIGS}")
        endif()
    else()
        message(FATAL_ERROR "Config '${cfg}' is not a PHOENIX config. Set PHX_CONFIG_MAP_${cfg} to one of: ${PHX_CONFIGS}")
    endif()
    string(TOUPPER ${mapped} mapped_upper)
    list(APPEND PHX_${mapped_upper}_CONFIGS ${cfg})
    string(TOUPPER ${cfg} cfg_upper)
    set(PHX_CONFIG_MAP_${cfg_upper} ${mapped})  # normalized lookup used by phx_map_imported_configs
endforeach()

# Generator-expression conditions: PHX_IS_<PHXCFG> is "$<CONFIG:a,b>" or "0" if nothing maps to it
foreach(phx_cfg IN LISTS PHX_CONFIGS)
    string(TOUPPER ${phx_cfg} phx_cfg_upper)
    if(PHX_${phx_cfg_upper}_CONFIGS)
        string(REPLACE ";" "," cfg_list "${PHX_${phx_cfg_upper}_CONFIGS}")
        set(PHX_IS_${phx_cfg_upper} "$<CONFIG:${cfg_list}>")
    else()
        set(PHX_IS_${phx_cfg_upper} "0")
    endif()
endforeach()

# Export paths, path segments, the build-config list and the mapped config
# conditions to consumers so phx_set_output_dirs()/phx_copy_slang_dlls() work
# from their scope
if(NOT PROJECT_IS_TOP_LEVEL)
    set(PHX_ROOT          "${PHX_ROOT}"          PARENT_SCOPE)
    set(PHX_ARCH_DIR      "${PHX_ARCH_DIR}"      PARENT_SCOPE)
    set(PHX_SYSTEM_DIR    "${PHX_SYSTEM_DIR}"    PARENT_SCOPE)
    set(PHX_BUILD_CONFIGS "${PHX_BUILD_CONFIGS}" PARENT_SCOPE)
    set(PHX_IS_DEBUG      "${PHX_IS_DEBUG}"      PARENT_SCOPE)
    set(PHX_IS_RELEASE    "${PHX_IS_RELEASE}"    PARENT_SCOPE)
    set(PHX_IS_SANITIZER  "${PHX_IS_SANITIZER}"  PARENT_SCOPE)
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
# Sets per-config ARCHIVE/RUNTIME/LIBRARY output directories for a target
function(phx_set_output_dirs target base_dir)
    foreach(cfg IN LISTS PHX_BUILD_CONFIGS)
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

# Maps each non-PHOENIX build config of an imported target onto the PHOENIX
# config it was mapped to, so the right prebuilt binary gets picked
function(phx_map_imported_configs target)
    foreach(cfg IN LISTS PHX_BUILD_CONFIGS)
        if(NOT cfg IN_LIST PHX_CONFIGS)
            string(TOUPPER ${cfg} cfg_upper)
            set_property(TARGET ${target} PROPERTY MAP_IMPORTED_CONFIG_${cfg_upper} ${PHX_CONFIG_MAP_${cfg_upper}})
        endif()
    endforeach()
endfunction()

# Copies the Slang runtime DLLs next to a target's output after build. The
# config passed to the script is the PHOENIX config the build config maps to,
# so consumers with their own config names get the right (d-suffixed) DLLs
function(phx_copy_slang_dlls target)
    set(phx_cfg "$<IF:$<OR:${PHX_IS_DEBUG},${PHX_IS_SANITIZER}>,Debug,Release>")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/copy_slang_dll.bat"
                "$<TARGET_FILE_DIR:${target}>"
                "${phx_cfg}"
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
