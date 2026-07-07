workspace "PHOENIX"

	architecture "x64"
	startproject "ImGui"
	
	configurations
	{
		"Debug",
		"Release",
		"Sanitizer"
	}
	
outputDir = "%{cfg.system}/%{cfg.buildcfg}/%{cfg.architecture}"

-- Core lib
include "PHOENIX/premake5.lua"

-- Samples
group "Samples"
include "samples/HelloTriangle/premake5.lua"
include "samples/BasicModel/premake5.lua"
include "samples/TexturedModel/premake5.lua"
include "samples/ComputeParticles/premake5.lua"
include "samples/ImGui/premake5.lua"
group ""