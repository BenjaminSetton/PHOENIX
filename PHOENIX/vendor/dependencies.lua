
VULKAN_SDK = os.getenv("VULKAN_SDK")

PHX_IncludeDirs                                      = {}
PHX_IncludeDirs["dep_vulkan"]                        = "%{wks.location}/PHOENIX/vendor/vulkan/include"
PHX_IncludeDirs["dep_glfw"]                          = "%{wks.location}/PHOENIX/vendor/glfw/include"
PHX_IncludeDirs["dep_vma"]                           = "%{wks.location}/PHOENIX/vendor/vma/include"
PHX_IncludeDirs["dep_glslang"]                       = "%{wks.location}/PHOENIX/vendor/glslang"
PHX_IncludeDirs["lib_inc"]                           = "%{wks.location}/PHOENIX/src/include"

PHX_Libraries                                        = {}
PHX_Libraries["vulkan"]                              = "%{VULKAN_SDK}/Lib/vulkan-1.lib"
PHX_Libraries["glfw_debug"]                          = "%{wks.location}/PHOENIX/out/glfw/src/debug/glfw3.lib"
PHX_Libraries["glslang_debug"]                       = "%{wks.location}/PHOENIX/out/glslang/glslang/debug/glslangd.lib"
PHX_Libraries["SPV_debug"]                           = "%{wks.location}/PHOENIX/out/glslang/SPIRV/debug/SPIRVd.lib"
PHX_Libraries["SPV_tools_debug"]                     = "%{wks.location}/PHOENIX/out/glslang/External/spirv-tools/source/Debug/SPIRV-Toolsd.lib"
PHX_Libraries["SPV_tools_opt_debug"]                 = "%{wks.location}/PHOENIX/out/glslang/External/spirv-tools/source/opt/Debug/SPIRV-Tools-optd.lib"
PHX_Libraries["glslang_code_gen_debug"]              = "%{wks.location}/PHOENIX/out/glslang/glslang/Debug/GenericCodeGend.lib"
PHX_Libraries["glslang_machine_independent_debug"]   = "%{wks.location}/PHOENIX/out/glslang/glslang/Debug/MachineIndependentd.lib"
PHX_Libraries["glslang_os_dependent_win_debug"]      = "%{wks.location}/PHOENIX/out/glslang/glslang/OSDependent/Windows/Debug/OSDependentd.lib"

PHX_Libraries["glfw_release"]                        = "%{wks.location}/PHOENIX/out/glfw/src/release/glfw3.lib"
PHX_Libraries["glslang_release"]                     = "%{wks.location}/PHOENIX/out/glslang/glslang/release/glslang.lib"
PHX_Libraries["SPV_release"]                         = "%{wks.location}/PHOENIX/out/glslang/SPIRV/release/SPIRV.lib"
PHX_Libraries["SPV_tools_release"]                   = "%{wks.location}/PHOENIX/out/glslang/External/spirv-tools/source/Release/SPIRV-Tools.lib"
PHX_Libraries["SPV_tools_opt_release"]               = "%{wks.location}/PHOENIX/out/glslang/External/spirv-tools/source/opt/Release/SPIRV-Tools-opt.lib"
PHX_Libraries["glslang_code_gen_release"]            = "%{wks.location}/PHOENIX/out/glslang/glslang/Release/GenericCodeGen.lib"
PHX_Libraries["glslang_machine_independent_release"] = "%{wks.location}/PHOENIX/out/glslang/glslang/Release/MachineIndependent.lib"
PHX_Libraries["glslang_os_dependent_win_release"]    = "%{wks.location}/PHOENIX/out/glslang/glslang/OSDependent/Windows/Release/OSDependent.lib"

