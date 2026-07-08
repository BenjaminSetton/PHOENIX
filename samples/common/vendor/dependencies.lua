
SamplesCommon_IncludeDirs                        = {}
SamplesCommon_IncludeDirs["PHOENIX"]             = "%{wks.location}/PHOENIX/src/api"
SamplesCommon_IncludeDirs["assimp_core"]         = "%{wks.location}/samples/common/vendor/assimp/include"
SamplesCommon_IncludeDirs["assimp_generated"]    = "%{wks.location}/samples/common/out/assimp/include"
SamplesCommon_IncludeDirs["glm"]                 = "%{wks.location}/samples/common/vendor/glm/glm"
SamplesCommon_IncludeDirs["stb_image"]           = "%{wks.location}/samples/common/vendor/stb_image"
SamplesCommon_IncludeDirs["imgui"]               = "%{wks.location}/samples/common/vendor/imgui"

SamplesCommon_Libraries                          = {}
SamplesCommon_Libraries["PHOENIX"]               = "%{wks.location}/PHOENIX/out/phx/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/PHOENIX.lib"
SamplesCommon_Libraries["assimp"]                = "%{wks.location}/samples/common/out/assimp/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/assimp-vc143-mt%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
SamplesCommon_Libraries["assimp_zlib"]           = "%{wks.location}/samples/common/out/assimp/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/zlibstatic%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
SamplesCommon_Libraries["glm"]                   = "%{wks.location}/samples/common/out/glm/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/glm.lib"

-- Copies the MSVC ASAN runtime DLL next to the built executable on Sanitizer builds.
-- Call this inside a sample's project block.
function SamplesCommon_CopyAsanDLL()
    filter { "system:windows", "configurations:Sanitizer" }
        postbuildcommands {
            '"%{wks.location}/utils/copy_asan_dll.bat" "%{cfg.targetdir}"'
        }
    filter {}
end