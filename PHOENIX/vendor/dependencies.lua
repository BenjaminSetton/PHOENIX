
VULKAN_SDK = os.getenv("VULKAN_SDK")

PHX_IncludeDirs                      = {}
PHX_IncludeDirs["dep_vulkan"]        = "%{wks.location}/PHOENIX/vendor/vulkan/include"
PHX_IncludeDirs["dep_glfw"]          = "%{wks.location}/PHOENIX/vendor/glfw/include"
PHX_IncludeDirs["dep_vma"]           = "%{wks.location}/PHOENIX/vendor/vma/include"
PHX_IncludeDirs["dep_shaderc"]       = "%{wks.location}/PHOENIX/vendor/shaderc/include"
PHX_IncludeDirs["lib_inc"]           = "%{wks.location}/PHOENIX/src/include"

PHX_Libraries                       = {}
PHX_Libraries["vulkan"]             = "%{VULKAN_SDK}/Lib/vulkan-1.lib"
PHX_Libraries["glfw"]               = "%{wks.location}/PHOENIX/vendor/glfw/out/bin/" .. outputDir .. "/glfw3.lib"
PHX_Libraries["shaderc_debug"]      = "%{wks.location}/PHOENIX/vendor/shaderc/bin/debug/shaderc_combinedd.lib"
PHX_Libraries["shaderc_release"]    = "%{wks.location}/PHOENIX/vendor/shaderc/bin/release/shaderc_combined.lib"
