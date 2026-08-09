# Shared build functions and CMake flags for PHOENIX dependencies.
# Dot-sourced by build_lib_dependencies.ps1, build_sample_dependencies.ps1, generate_project.ps1.

# ---------------------------------------------------------------------------
# Paths (replaces config.bat)
# ---------------------------------------------------------------------------
$UtilsDir    = $PSScriptRoot
$WorkspaceDir = Split-Path -Parent $UtilsDir
$LibOut      = "$WorkspaceDir/src/PHOENIX/out"
$SamplesOut  = "$WorkspaceDir/src/samples/common/out"

# ---------------------------------------------------------------------------
# Generator settings
# ---------------------------------------------------------------------------
# MSVC CMake toolset version (v143 = VS2022, v142 = VS2019)
$CmakeToolset = 'v143'

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Log([string]$msg) { Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $msg" }

# Maps Premake architecture names (x86_64, arm64) to CMake -A values (x64, ARM64).
function ConvertTo-CMakeArch {
    param([string]$Architecture)
    switch ($Architecture) {
        'x86_64' { return 'x64' }
        'arm64'  { return 'ARM64' }
        default  { return $Architecture }
    }
}

function Resolve-Configs {
    param([string]$Config, [bool]$IncludeSanitizer)
    if ($Config -eq 'All') {
        if ($IncludeSanitizer) { return @('Debug', 'Release', 'Sanitizer') }
        else                   { return @('Debug', 'Release') }
    }
    if ($Config -eq 'Sanitizer' -and -not $IncludeSanitizer) { return @() }
    return @($Config)
}

# ---------------------------------------------------------------------------
# GLFW
# ---------------------------------------------------------------------------
function Build-GLFW {
    param(
        [string]$Config,
        [string]$Architecture
    )

    $configs = Resolve-Configs -Config $Config -IncludeSanitizer $true
    $cmakeArch = ConvertTo-CMakeArch $Architecture
    $src = "$WorkspaceDir/src/PHOENIX/vendor/glfw"
    $out = "$LibOut/glfw"

    Log 'Building GLFW...'
    & cmake --fresh -A $cmakeArch -T $CmakeToolset -S $src -B $out `
        '-DGLFW_BUILD_EXAMPLES=OFF' `
        '-DGLFW_BUILD_TESTS=OFF' `
        '-DGLFW_BUILD_DOCS=OFF' `
        '-DCMAKE_CONFIGURATION_TYPES=Debug;Release;Sanitizer' `
        '-DCMAKE_C_FLAGS_DEBUG=/Z7 /Ob0 /Od /RTC1' `
        '-DCMAKE_C_FLAGS_SANITIZER=/Z7 /Ob0 /Od /RTC1' `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=$LibOut/glfw/bin/windows/debug/$Architecture" `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=$LibOut/glfw/bin/windows/release/$Architecture" `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_SANITIZER=$LibOut/glfw/bin/windows/sanitizer/$Architecture" `
        '-DCMAKE_DEBUG_POSTFIX=d' `
        '-DCMAKE_SANITIZER_POSTFIX=d' `
        '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Sanitizer>:Debug>DLL'
    if ($LASTEXITCODE -ne 0) { throw 'GLFW CMake configure failed' }

    foreach ($cfg in $configs) {
        Log "  Building GLFW $cfg..."
        & cmake --build $out --config $cfg --parallel
        if ($LASTEXITCODE -ne 0) { throw "GLFW build failed for $cfg" }
    }
    Log 'GLFW build complete.'
}

# ---------------------------------------------------------------------------
# Slang  (Debug + Release; Sanitizer links against Debug Slang)
# ---------------------------------------------------------------------------
function Build-Slang {
    param(
        [string]$Config,
        [string]$Architecture
    )

    $configs = Resolve-Configs -Config $Config -IncludeSanitizer $false
    if ($configs.Count -eq 0) {
        Log 'Sanitizer config: building Debug Slang (used at link time for Sanitizer).'
        $configs = @('Debug')
    }

    $cmakeArch = ConvertTo-CMakeArch $Architecture
    $src = "$WorkspaceDir/src/PHOENIX/vendor/slang"
    $out = "$LibOut/slang"

    Log 'Pulling Slang dependencies...'
    & git -C "$WorkspaceDir/src/PHOENIX/vendor/slang" submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw 'Slang submodule init failed' }

    Log 'Configuring Slang...'
    & cmake --fresh -A $cmakeArch -T $CmakeToolset -S $src -B $out `
        '-DSLANG_LIB_TYPE=SHARED' `
        '-DSLANG_ENABLE_TESTS=OFF' `
        '-DSLANG_ENABLE_EXAMPLES=OFF' `
        '-DSLANG_ENABLE_SLANGD=OFF' `
        '-DSLANG_ENABLE_SLANGC=OFF' `
        '-DSLANG_ENABLE_SLANGI=OFF' `
        '-DSLANG_ENABLE_SLANGRT=OFF' `
        '-DSLANG_ENABLE_GFX=OFF' `
        '-DSLANG_ENABLE_SLANG_PROXY=OFF' `
        '-DSLANG_ENABLE_SLANG_RHI=OFF' `
        '-DSLANG_ENABLE_CUDA=OFF' `
        '-DSLANG_ENABLE_OPTIX=OFF' `
        '-DSLANG_ENABLE_NVAPI=OFF' `
        '-DSLANG_ENABLE_AFTERMATH=OFF' `
        '-DSLANG_ENABLE_DXIL=OFF' `
        '-DSLANG_ENABLE_SLANG_GLSLANG=ON' `
        '-DCMAKE_CONFIGURATION_TYPES=Debug;Release' `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=$LibOut/slang/bin/windows/debug/$Architecture" `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=$LibOut/slang/bin/windows/release/$Architecture" `
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=$LibOut/slang/bin/windows/debug/$Architecture" `
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=$LibOut/slang/bin/windows/release/$Architecture" `
        "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG=$LibOut/slang/bin/windows/debug/$Architecture" `
        "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE=$LibOut/slang/bin/windows/release/$Architecture" `
        '-DCMAKE_DEBUG_POSTFIX=d' `
        '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL'
    if ($LASTEXITCODE -ne 0) { throw 'Slang CMake configure failed' }

    foreach ($cfg in $configs) {
        Log "  Building Slang $cfg (bootstrap)..."
        & cmake --build $out --config $cfg --parallel --target slang-bootstrap slang-without-embedded-core-module
        if ($LASTEXITCODE -ne 0) { throw "Slang bootstrap build failed for $cfg" }

        Log "  Building Slang $cfg (full)..."
        & cmake --build $out --config $cfg --parallel --target slang slang-embedded-core-module-source slang-glslang slang-glsl-module
        if ($LASTEXITCODE -ne 0) { throw "Slang full build failed for $cfg" }
    }
    Log 'Slang build complete.'
}

# ---------------------------------------------------------------------------
# Assimp
# ---------------------------------------------------------------------------
function Build-Assimp {
    param(
        [string]$Config,
        [string]$Architecture
    )

    $configs = Resolve-Configs -Config $Config -IncludeSanitizer $true
    $cmakeArch = ConvertTo-CMakeArch $Architecture
    $src = "$WorkspaceDir/src/samples/common/vendor/assimp"
    $out = "$SamplesOut/assimp"

    Log 'Building assimp...'
    & cmake --fresh -A $cmakeArch -T $CmakeToolset -S $src -B $out `
        '-DBUILD_SHARED_LIBS=OFF' `
        '-DASSIMP_BUILD_TESTS=OFF' `
        '-DASSIMP_BUILD_ZLIB=ON' `
        '-DASSIMP_BUILD_ASSIMP_TOOLS=OFF' `
        '-DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF' `
        '-DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF' `
        '-DASSIMP_BUILD_FBX_IMPORTER=ON' `
        '-DASSIMP_BUILD_GLTF_IMPORTER=ON' `
        '-DASSIMP_BUILD_GLTF2_IMPORTER=ON' `
        '-DASSIMP_BUILD_OBJ_IMPORTER=ON' `
        '-DLIBRARY_SUFFIX=-mt' `
        '-DCMAKE_CONFIGURATION_TYPES=Debug;Release;Sanitizer' `
        '-DCMAKE_C_FLAGS_SANITIZER=/fsanitize=address /Zi' `
        '-DCMAKE_CXX_FLAGS_SANITIZER=/fsanitize=address /Zi' `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=$SamplesOut/assimp/bin/windows/$Architecture" `
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$SamplesOut/assimp/bin/windows/$Architecture" `
        "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=$SamplesOut/assimp/bin/windows/$Architecture" `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=$SamplesOut/assimp/bin/windows/debug/$Architecture" `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=$SamplesOut/assimp/bin/windows/release/$Architecture" `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_SANITIZER=$SamplesOut/assimp/bin/windows/sanitizer/$Architecture" `
        '-DCMAKE_SANITIZER_POSTFIX=d' `
        '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Sanitizer>:Debug>DLL'
    if ($LASTEXITCODE -ne 0) { throw 'assimp CMake configure failed' }

    foreach ($cfg in $configs) {
        Log "  Building assimp $cfg..."
        & cmake --build $out --config $cfg --parallel
        if ($LASTEXITCODE -ne 0) { throw "assimp build failed for $cfg" }
    }
    Log 'assimp build complete.'
}

# ---------------------------------------------------------------------------
# GLM
# ---------------------------------------------------------------------------
function Build-GLM {
    param(
        [string]$Config,
        [string]$Architecture
    )

    $configs = Resolve-Configs -Config $Config -IncludeSanitizer $true
    $cmakeArch = ConvertTo-CMakeArch $Architecture
    $src = "$WorkspaceDir/src/samples/common/vendor/glm"
    $out = "$SamplesOut/glm"

    Log 'Building glm...'
    & cmake --fresh -A $cmakeArch -T $CmakeToolset -S $src -B $out `
        '-DGLM_BUILD_LIBRARY=ON' `
        '-DGLM_BUILD_TESTS=OFF' `
        '-DCMAKE_CONFIGURATION_TYPES=Debug;Release;Sanitizer' `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=$SamplesOut/glm/bin/windows/debug/$Architecture" `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=$SamplesOut/glm/bin/windows/release/$Architecture" `
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_SANITIZER=$SamplesOut/glm/bin/windows/sanitizer/$Architecture" `
        '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Sanitizer>:Debug>DLL'
    if ($LASTEXITCODE -ne 0) { throw 'glm CMake configure failed' }

    foreach ($cfg in $configs) {
        Log "  Building glm $cfg..."
        & cmake --build $out --config $cfg --parallel
        if ($LASTEXITCODE -ne 0) { throw "glm build failed for $cfg" }
    }
    Log 'glm build complete.'
}

# ---------------------------------------------------------------------------
# Premake / VS solution generation
# ---------------------------------------------------------------------------
function Generate-Project {
    param(
        [string]$Generator,
        [string]$Architecture
    )
    Log "Generating solution for $Generator on $Architecture..."
    $env:PHX_ARCH = $Architecture
    Push-Location $WorkspaceDir
    try {
        & "$WorkspaceDir/src/PHOENIX/vendor/premake/premake5.exe" $Generator
        if ($LASTEXITCODE -ne 0) { throw 'Premake failed' }
    } finally {
        Pop-Location
    }
    Log 'Solution generated.'
}
