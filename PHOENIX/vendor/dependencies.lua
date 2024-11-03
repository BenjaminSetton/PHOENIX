
VULKAN_SDK = os.getenv("VULKAN_SDK")

PHX_IncludeDirs                     = {}
PHX_IncludeDirs["vulkan"]           = "%{wks.location}/PHOENIX/vendor/vulkan/include"
PHX_IncludeDirs["glfw"]             = "%{wks.location}/PHOENIX/vendor/glfw/include"
PHX_IncludeDirs["force_include"]    = "%{wks.location}/PHOENIX/src/utils/force_include"

PHX_Libraries                       = {}
PHX_Libraries["vulkan"]             = "%{VULKAN_SDK}/Lib/vulkan-1.lib"
PHX_Libraries["glfw_debug"]         = "%{wks.location}/PHOENIX/vendor/glfw/bin/debug/glfw3.lib"
PHX_Libraries["glfw_release"]       = "%{wks.location}/PHOENIX/vendor/glfw/bin/release/glfw3.lib"
