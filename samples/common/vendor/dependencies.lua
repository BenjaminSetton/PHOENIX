
SamplesCommon_IncludeDirs                        = {}
SamplesCommon_IncludeDirs["PHOENIX"]             = "%{wks.location}/PHOENIX/src/api"
SamplesCommon_IncludeDirs["assimp_core"]         = "%{wks.location}/samples/common/vendor/assimp/include"
SamplesCommon_IncludeDirs["assimp_generated"]    = "%{wks.location}/samples/common/out/assimp/include"
SamplesCommon_IncludeDirs["glm"]                 = "%{wks.location}/samples/common/vendor/glm/glm"
SamplesCommon_IncludeDirs["stb_image"]           = "%{wks.location}/samples/common/vendor/stb_image"

SamplesCommon_Libraries                          = {}
SamplesCommon_Libraries["PHOENIX_win64_debug"]   = "%{wks.location}/PHOENIX/out/PHX/bin/windows/Debug/x86_64/PHOENIX.lib"
SamplesCommon_Libraries["PHOENIX_win64_release"] = "%{wks.location}/PHOENIX/out/PHX/bin/windows/Release/x86_64/PHOENIX.lib"
SamplesCommon_Libraries["assimp_debug"]          = "%{wks.location}/samples/common/out/assimp/lib/Debug/assimp-vc143-mtd.lib"
SamplesCommon_Libraries["assimp_release"]        = "%{wks.location}/samples/common/out/assimp/lib/Release/assimp-vc143-mt.lib"
SamplesCommon_Libraries["assimp_zlib_debug"]     = "%{wks.location}/samples/common/out/assimp/contrib/zlib/Debug/zlibstaticd.lib"
SamplesCommon_Libraries["assimp_zlib_release"]   = "%{wks.location}/samples/common/out/assimp/contrib/zlib/Release/zlibstatic.lib"
SamplesCommon_Libraries["glm_debug"]             = "%{wks.location}/samples/common/out/glm/glm/Debug/glm.lib"
SamplesCommon_Libraries["glm_release"]           = "%{wks.location}/samples/common/out/glm/glm/Release/glm.lib"