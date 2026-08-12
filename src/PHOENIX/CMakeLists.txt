# Change PHX_SHARED_LIB to ON to build as a DLL instead of a static lib
option(PHX_SHARED_LIB "Build PHX as a shared library" OFF)

# Tracy profiler
set(TRACY_ROOT "${PHX_ROOT}/vendor/tracy/public")
set(TRACY_CLIENT_SOURCE "${TRACY_ROOT}/TracyClient.cpp")

file(GLOB_RECURSE PHX_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(PHX_SHARED_LIB)
    add_library(PHOENIX SHARED ${PHX_SOURCES} ${TRACY_CLIENT_SOURCE})
    target_compile_definitions(PHOENIX PRIVATE PHX_BUILDING_LIB PHX_DYNAMIC_LIB)
else()
    add_library(PHOENIX STATIC ${PHX_SOURCES} ${TRACY_CLIENT_SOURCE})
endif()

# Mirror the directory structure
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${PHX_SOURCES})

target_include_directories(PHOENIX
    PUBLIC
        "${PHX_ROOT}/src/api"
    PRIVATE
        "${PHX_ROOT}/src/lib"
        "${TRACY_ROOT}"
)

# Link dependencies must be PUBLIC for a static library: the static lib doesn't
# resolve symbols itself, so the consumer (samples) must link them at the final
# executable link step. PUBLIC propagates both the libs and their interface
# include dirs to consumers.
target_link_libraries(PHOENIX PUBLIC
    phx_vulkan
    phx_glfw
    phx_slang
    phx_vma
    bsl
)

# Per-config defines
target_compile_definitions(PHOENIX PRIVATE
    $<$<CONFIG:Debug>:PHX_DEBUG>
    $<$<CONFIG:Debug>:BSL_ENABLE_DEBUG_LOG>
    $<$<CONFIG:Release>:PHX_RELEASE>
    $<$<CONFIG:Sanitizer>:PHX_DEBUG>
    $<$<CONFIG:Sanitizer>:PHX_SANITIZE>
    $<$<CONFIG:Sanitizer>:BSL_ENABLE_DEBUG_LOG>
    
    # Profiling (only in Release)
	$<$<CONFIG:Release>:TRACY_ENABLE>
    $<$<CONFIG:Release>:PROFILER_TRACY>
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(PHOENIX PRIVATE PHX_WINDOWS)
endif()

# Extra warnings on every platform
if(MSVC)
    target_compile_options(PHOENIX PRIVATE /W4)
else()
    target_compile_options(PHOENIX PRIVATE -Wall -Wextra)
endif()

# Output directories
phx_set_output_dirs(PHOENIX "${PHX_ROOT}/out/PHX/bin")
