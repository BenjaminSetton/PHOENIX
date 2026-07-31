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

-- Core lib
include "PHOENIX/premake5.lua"

-- Samples
group "Samples"
include "samples/HelloTriangle/premake5.lua"
include "samples/RayTracing/premake5.lua"
include "samples/BasicModel/premake5.lua"
include "samples/TexturedModel/premake5.lua"
include "samples/ComputeParticles/premake5.lua"
include "samples/ImGui/premake5.lua"
include "samples/InstancedAnimation/premake5.lua"
include "samples/Tessellation/premake5.lua"
group ""