
SamplesCommon_IncludeDirs                        = {}
SamplesCommon_IncludeDirs["PHOENIX"]             = "%{wks.location}/%{PHX_ROOT}/src/api"
SamplesCommon_IncludeDirs["BSL"]                 = "%{wks.location}/%{BSL_ROOT}"
SamplesCommon_IncludeDirs["assimp_core"]         = "%{wks.location}/%{SAMPLES_ROOT}/common/vendor/assimp/include"
SamplesCommon_IncludeDirs["assimp_generated"]    = "%{wks.location}/%{SAMPLES_ROOT}/common/out/assimp/include"
SamplesCommon_IncludeDirs["glm"]                 = "%{wks.location}/%{SAMPLES_ROOT}/common/vendor/glm/glm"
SamplesCommon_IncludeDirs["stb_image"]           = "%{wks.location}/%{SAMPLES_ROOT}/common/vendor/stb_image"
SamplesCommon_IncludeDirs["imgui"]               = "%{wks.location}/%{SAMPLES_ROOT}/common/vendor/imgui"

SamplesCommon_Libraries                          = {}
SamplesCommon_Libraries["PHOENIX"]               = "%{wks.location}/%{PHX_ROOT}/out/phx/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/PHOENIX.lib"
SamplesCommon_Libraries["assimp"]                = "%{wks.location}/%{SAMPLES_ROOT}/common/out/assimp/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/assimp-mt%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
SamplesCommon_Libraries["assimp_zlib"]           = "%{wks.location}/%{SAMPLES_ROOT}/common/out/assimp/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/zlibstatic%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
SamplesCommon_Libraries["glm"]                   = "%{wks.location}/%{SAMPLES_ROOT}/common/out/glm/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/glm.lib"

-- Build-time path defines for asset import/load system
-- COMMON_ASSET_ROOT_DIR: shared assets across all samples
-- SAMPLE_ASSET_ROOT_DIR: sample-specific assets (per project)
-- CACHE_ROOT_DIR: serialized cache files (per project)
-- Call this INSIDE a project block so %{prj.name} resolves correctly.
function SamplesCommon_SetAssetDefines()
    defines {
        'COMMON_ASSET_ROOT_DIR="%{wks.location}/%{SAMPLES_ROOT}/common/assets"',
        'SAMPLE_ASSET_ROOT_DIR="%{wks.location}/%{SAMPLES_ROOT}/%{prj.name}/assets"',
        'CACHE_ROOT_DIR="%{wks.location}/%{SAMPLES_ROOT}/%{prj.name}/out/cache"'
    }
end

-- Copies runtime DLLs (ASAN, Slang, etc) next to the built executable
-- Call this inside a sample's project block
function SamplesCommon_CopyDLLs()
    filter { "system:windows", "configurations:Sanitizer" }
        postbuildcommands {
            'call "%{wks.location}/utils/copy_asan_dll.bat" "%{cfg.targetdir}"'
        }
    filter { "system:windows" }
        postbuildcommands {
            'call "%{wks.location}/utils/copy_slang_dll.bat" "%{cfg.targetdir}" "%{cfg.buildcfg}" "%{wks.location}/%{PHX_ROOT}/out/slang/bin/%{cfg.system}/%{slangConfigLower[cfg.buildcfg]}/%{cfg.architecture}"'
        }
    filter {}
end