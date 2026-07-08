
VULKAN_SDK = os.getenv("VULKAN_SDK")

PHX_IncludeDirs                                      = {}
PHX_IncludeDirs["dep_vulkan"]                        = "%{wks.location}/PHOENIX/vendor/vulkan/include"
PHX_IncludeDirs["dep_glfw"]                          = "%{wks.location}/PHOENIX/vendor/glfw/include"
PHX_IncludeDirs["dep_vma"]                           = "%{wks.location}/PHOENIX/vendor/vma/include"
PHX_IncludeDirs["dep_glslang"]                       = "%{wks.location}/PHOENIX/vendor/glslang"
PHX_IncludeDirs["inc_api"]                           = "%{wks.location}/PHOENIX/src/api"
PHX_IncludeDirs["inc_lib"]                           = "%{wks.location}/PHOENIX/src/lib"

PHX_Libraries                                        = {}
PHX_Libraries["vulkan"]                              = "%{VULKAN_SDK}/Lib/vulkan-1.lib"
PHX_Libraries["glfw"]                                = "%{wks.location}/PHOENIX/out/glfw/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/glfw3%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
PHX_Libraries["glslang"]                             = "%{wks.location}/PHOENIX/out/glslang/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/glslang%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
PHX_Libraries["SPV"]                                 = "%{wks.location}/PHOENIX/out/glslang/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/SPIRV%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
PHX_Libraries["glslang_code_gen"]                    = "%{wks.location}/PHOENIX/out/glslang/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/GenericCodeGen%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
PHX_Libraries["glslang_machine_independent"]         = "%{wks.location}/PHOENIX/out/glslang/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/MachineIndependent%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
PHX_Libraries["glslang_os_dependent_win"]            = "%{wks.location}/PHOENIX/out/glslang/bin/%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}/OSDependent%{ConfigMap.debugSuffix[cfg.buildcfg]}.lib"
