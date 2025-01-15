
VULKAN_SDK = os.getenv("VULKAN_SDK")

PHX_IncludeDirs                      = {}
PHX_IncludeDirs["dep_vulkan"]        = "%{wks.location}/PHOENIX/vendor/vulkan/include"
PHX_IncludeDirs["dep_glfw"]          = "%{wks.location}/PHOENIX/vendor/glfw/include"
PHX_IncludeDirs["dep_vma"]           = "%{wks.location}/PHOENIX/vendor/vma/include"
PHX_IncludeDirs["dep_glslang"]       = "%{wks.location}/PHOENIX/vendor/glslang/glslang/include"
PHX_IncludeDirs["lib_inc"]           = "%{wks.location}/PHOENIX/src/include"

PHX_Libraries                        = {}
PHX_Libraries["vulkan"]              = "%{VULKAN_SDK}/Lib/vulkan-1.lib"
PHX_Libraries["glfw_debug"]          = "%{wks.location}/PHOENIX/vendor/glfw/build/src/debug/glfw3.lib"
PHX_Libraries["glfw_release"]        = "%{wks.location}/PHOENIX/vendor/glfw/build/src/release/glfw3.lib"
PHX_Libraries["glslang_debug"]       = "%{wks.location}/PHOENIX/vendor/glslang/build/glslang/debug/glslangd.lib"
PHX_Libraries["glslang_release"]       = "%{wks.location}/PHOENIX/vendor/glslang/build/glslang/release/glslang.lib"