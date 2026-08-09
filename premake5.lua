local function GetArchitecture()
	-- os.hostarch() doesn't seem to be working as intended for detecting WoA, instead
	-- returning x86_64. We'll hack our way around that for now:
	-- https://github.com/premake/premake-core/issues/2472
	local hostArch = os.hostarch()
	if os.host() == "windows" and os.isdir("C:/Windows/SysArm64") then
		hostArch = "arm64"
	end

	print("Using architecture \"" .. hostArch .. "\"")
	return hostArch
end

workspace "PHOENIX"

	architecture(GetArchitecture())
	startproject "ImGui"

	-- Enable multi-threaded builds
	multiprocessorcompile "On"

	configurations
	{
		"Debug",
		"Release",
		"Sanitizer"
	}
	
configLower = { Debug = "debug", Release = "release", Sanitizer = "sanitizer" }
slangConfigLower = { Debug = "debug", Release = "release", Sanitizer = "debug" }
ConfigMap = {
    debugSuffix = { Debug = "d", Release = "", Sanitizer = "d" }
}

outputDir = "%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}"

-- Sub-project root paths relative to the workspace directory
PHX_ROOT     = "src/PHOENIX"
SAMPLES_ROOT = "src/samples"
BSL_ROOT     = "src/BSL"

-- Core lib
include(PHX_ROOT .. "/premake5.lua")

-- Samples
group "Samples"
include(SAMPLES_ROOT .. "/HelloTriangle/premake5.lua")
include(SAMPLES_ROOT .. "/RayTracing/premake5.lua")
include(SAMPLES_ROOT .. "/BasicModel/premake5.lua")
include(SAMPLES_ROOT .. "/TexturedModel/premake5.lua")
include(SAMPLES_ROOT .. "/ComputeParticles/premake5.lua")
include(SAMPLES_ROOT .. "/ImGui/premake5.lua")
include(SAMPLES_ROOT .. "/InstancedAnimation/premake5.lua")
include(SAMPLES_ROOT .. "/Tessellation/premake5.lua")
include(SAMPLES_ROOT .. "/Lod/premake5.lua")
group ""