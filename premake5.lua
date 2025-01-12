workspace "PHOENIX"

	architecture "x64"
	startproject "SceneDemo"
	
	configurations
	{
		"Debug",
		"Release"
	}
	
outputDir = "%{cfg.system}/%{cfg.buildcfg}/%{cfg.architecture}"

-- Projects dependencies
group "Deps"
include "PHOENIX/vendor/glfw/premake5.lua"
include "PHOENIX/vendor/shaderc-phx/premake5.lua"
include "PHOENIX/vendor/shaderc-phx/third_party/spirv-tools/premake5.lua"
group ""

-- Core lib
include "PHOENIX/premake5.lua"

-- Samples
group "Samples"
include "SceneDemo/premake5.lua"
group ""