workspace "PHOENIX"

	architecture "x64"
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