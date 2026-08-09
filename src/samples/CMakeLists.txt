# Assimp
add_library(assimp STATIC IMPORTED)
set_target_properties(assimp PROPERTIES
    IMPORTED_LOCATION_DEBUG     "${SAMPLES_ROOT}/common/out/assimp/bin/${PHX_SYSTEM_DIR}/debug/${PHX_ARCH_DIR}/assimp-mtd.lib"
    IMPORTED_LOCATION_RELEASE   "${SAMPLES_ROOT}/common/out/assimp/bin/${PHX_SYSTEM_DIR}/release/${PHX_ARCH_DIR}/assimp-mt.lib"
    IMPORTED_LOCATION_SANITIZER "${SAMPLES_ROOT}/common/out/assimp/bin/${PHX_SYSTEM_DIR}/sanitizer/${PHX_ARCH_DIR}/assimp-mtd.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SAMPLES_ROOT}/common/vendor/assimp/include;${SAMPLES_ROOT}/common/out/assimp/include"
)

# Assimp's zlib dependency
add_library(assimp_zlib STATIC IMPORTED)
set_target_properties(assimp_zlib PROPERTIES
    IMPORTED_LOCATION_DEBUG     "${SAMPLES_ROOT}/common/out/assimp/bin/${PHX_SYSTEM_DIR}/debug/${PHX_ARCH_DIR}/zlibstaticd.lib"
    IMPORTED_LOCATION_RELEASE   "${SAMPLES_ROOT}/common/out/assimp/bin/${PHX_SYSTEM_DIR}/release/${PHX_ARCH_DIR}/zlibstatic.lib"
    IMPORTED_LOCATION_SANITIZER "${SAMPLES_ROOT}/common/out/assimp/bin/${PHX_SYSTEM_DIR}/sanitizer/${PHX_ARCH_DIR}/zlibstaticd.lib"
)

# GLM — header-only
add_library(glm STATIC IMPORTED)
set_target_properties(glm PROPERTIES
    IMPORTED_LOCATION_DEBUG     "${SAMPLES_ROOT}/common/out/glm/bin/${PHX_SYSTEM_DIR}/debug/${PHX_ARCH_DIR}/glm.lib"
    IMPORTED_LOCATION_RELEASE   "${SAMPLES_ROOT}/common/out/glm/bin/${PHX_SYSTEM_DIR}/release/${PHX_ARCH_DIR}/glm.lib"
    IMPORTED_LOCATION_SANITIZER "${SAMPLES_ROOT}/common/out/glm/bin/${PHX_SYSTEM_DIR}/sanitizer/${PHX_ARCH_DIR}/glm.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SAMPLES_ROOT}/common/vendor/glm/glm"
)

# stb_image — header-only
add_library(stb_image INTERFACE IMPORTED)
set_target_properties(stb_image PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SAMPLES_ROOT}/common/vendor/stb_image"
)

# ImGui — header-only
add_library(imgui INTERFACE IMPORTED)
set_target_properties(imgui PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SAMPLES_ROOT}/common/vendor/imgui"
)

# ---------------------------------------------------------------------------
# add_sample — creates a sample executable with shared config
# ---------------------------------------------------------------------------
# Usage: add_sample(<name> [INCLUDE_COMMON_SHADERS])
function(add_sample name)
    set(options INCLUDE_COMMON_SHADERS)
    cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})

    file(GLOB_RECURSE SAMPLE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*"
    )

    add_executable(${name} ${SAMPLE_SOURCES})

    # ImGui sources (compiled into every sample)
    set(IMGUI_SOURCES
        "${SAMPLES_ROOT}/common/vendor/imgui/imgui.cpp"
        "${SAMPLES_ROOT}/common/vendor/imgui/imgui_draw.cpp"
        "${SAMPLES_ROOT}/common/vendor/imgui/imgui_tables.cpp"
        "${SAMPLES_ROOT}/common/vendor/imgui/imgui_widgets.cpp"
        "${SAMPLES_ROOT}/common/vendor/imgui/imgui_demo.cpp"
    )
    target_sources(${name} PRIVATE ${IMGUI_SOURCES})

    # Common sample sources
    file(GLOB_RECURSE COMMON_SOURCES
        "${SAMPLES_ROOT}/common/src/*.h"
        "${SAMPLES_ROOT}/common/src/*.cpp"
    )
    target_sources(${name} PRIVATE ${COMMON_SOURCES})

    # Common shaders (optional)
    if(ARG_INCLUDE_COMMON_SHADERS)
        file(GLOB_RECURSE COMMON_SHADER_SOURCES
            "${SAMPLES_ROOT}/common/src/shaders/*"
        )
        target_sources(${name} PRIVATE ${COMMON_SHADER_SOURCES})
    endif()

    # Mirror directory structure in VS Solution Explorer
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${SAMPLE_SOURCES})
    source_group(TREE "${SAMPLES_ROOT}/common/src" FILES ${COMMON_SOURCES} ${COMMON_SHADER_SOURCES})
    source_group(TREE "${SAMPLES_ROOT}/common/vendor/imgui" FILES ${IMGUI_SOURCES})

    # Include directories
    # Note: "PHOENIX/src/api/..." is inherited through target_link_libraries(PHOENIX)
    target_include_directories(${name} PRIVATE
        "${BSL_ROOT}"
        "${SAMPLES_ROOT}/common/vendor/glm/glm"
        "${SAMPLES_ROOT}/common/vendor/assimp/include"
        "${SAMPLES_ROOT}/common/out/assimp/include"
        "${SAMPLES_ROOT}/common/vendor/stb_image"
        "${SAMPLES_ROOT}/common/vendor/imgui"
    )

    # Link libraries
    target_link_libraries(${name} PRIVATE
        PHOENIX
        assimp_zlib
        assimp
        glm
        bsl
        stb_image
    )

    # Extra warnings on every platform
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra)
    endif()

    # Asset path defines
    target_compile_definitions(${name} PRIVATE
        "COMMON_ASSET_ROOT_DIR=\"${SAMPLES_ROOT}/common/assets\""
        "SAMPLE_ASSET_ROOT_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\""
        "CACHE_ROOT_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/out/cache\""
    )

    # Post-build - copy Slang DLLs
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND "${CMAKE_SOURCE_DIR}/utils/copy_slang_dll.bat"
                "$<TARGET_FILE_DIR:${name}>"
                "$<CONFIG>"
                "${PHX_ROOT}/out/slang/bin/${PHX_SYSTEM_DIR}/$<IF:$<CONFIG:Release>,release,debug>/${PHX_ARCH_DIR}"
        VERBATIM
    )

    # Post-build - copy ASAN DLL for Sanitizer config only
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND cmd /c "if /i \"$<CONFIG>\"==\"Sanitizer\" call \"${CMAKE_SOURCE_DIR}/utils/copy_asan_dll.bat\" \"$<TARGET_FILE_DIR:${name}>\""
    )

    # Output directories - out/bin/<system>/<config>/<arch>
    phx_set_output_dirs(${name} "${CMAKE_CURRENT_SOURCE_DIR}/out/bin")

    # Working directory - currently mirrors the old Premake config
    set_target_properties(${name} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FOLDER "Samples"
    )

    # Mark PHX as a core dependency
    add_dependencies(${name} PHOENIX)
endfunction()

# ---------------------------------------------------------------------------
# Samples
# ---------------------------------------------------------------------------
add_subdirectory(HelloTriangle)
add_subdirectory(RayTracing)
add_subdirectory(BasicModel)
add_subdirectory(TexturedModel)
add_subdirectory(ComputeParticles)
add_subdirectory(ImGui)
add_subdirectory(InstancedAnimation)
add_subdirectory(Tessellation)
add_subdirectory(Lod)
