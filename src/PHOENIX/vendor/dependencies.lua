
VULKAN_SDK = os.getenv("VULKAN_SDK")

PHX_IncludeDirs                                      = {}
PHX_IncludeDirs["dep_BSL"]                           = "%{wks.location}/%{BSL_ROOT}"
PHX_IncludeDirs["dep_vulkan"]                        = "%{wks.location}/%{PHX_ROOT}/vendor/vulkan/include"
PHX_IncludeDirs["dep_glfw"]                          = "%{wks.location}/%{PHX_ROOT}/vendor/glfw/include"
PHX_IncludeDirs["dep_vma"]                           = "%{wks.location}/%{PHX_ROOT}/vendor/vma/include"
PHX_IncludeDirs["dep_slang"]                         = "%{wks.location}/%{PHX_ROOT}/vendor/slang/include"
PHX_IncludeDirs["inc_api"]                           = "%{wks.location}/%{PHX_ROOT}/src/api"
PHX_IncludeDirs["inc_lib"]                           = "%{wks.location}/%{PHX_ROOT}/src/lib"

PHX_Libraries                                        = {}
PHX_Libraries["vulkan"]                              = "%{VULKAN_SDK}/Lib/vulkan-1.lib"
PHX_Libraries["glfw"]                                = "%{wks.location}/%{PHX_ROOT}/out/glfw/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/glfw3%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
PHX_Libraries["slang"]                               = "%{wks.location}/%{PHX_ROOT}/out/slang/bin/%{cfg.system}/%{slangConfigLower[cfg.buildcfg]}/%{cfg.architecture}/slang-compiler%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
