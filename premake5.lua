workspace "PHOENIX"

	architecture "x64"
	startproject "SceneDemo"
	
	configurations
	{
		"Debug",
		"Release"
	}

-- Projects
include "PHOENIX/premake5.lua"
include "SceneDemo/premake5.lua"