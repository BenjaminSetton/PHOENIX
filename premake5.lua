workspace "PHOENIX"

	architecture "x64"
	startproject "SceneDemo"
	
	configurations
	{
		"Debug",
		"Release"
	}
	
outputDir = "%{cfg.system}/%{cfg.buildcfg}/%{cfg.architecture}"

-- Core lib
include "PHOENIX/premake5.lua"

-- Samples
group "Samples"
include "SceneDemo/premake5.lua"
group ""