
include "vendor/dependencies.lua"

outputDir = "%{cfg.system}/%{cfg.buildcfg}/%{cfg.architecture}"

project "PHOENIX"
	location "out"
	language "C++"
	kind "StaticLib"
	
	targetdir ("out/bin/" .. outputDir)
	objdir ("out/obj/" .. outputDir)
	
	files
	{
		"src/**.h",
		"src/**.cpp"
	}
	
	includedirs
	{
		"%{TANG_IncludeDirs.vulkan}",
		"%{TANG_IncludeDirs.glfw}"
	}
	
	links
	{
		"%{TANG_Libraries.vulkan}"
	}
	
	filter "system:windows"
		cppdialect "C++14"
		systemversion "latest"
		warnings "High"
		defines "PHX_WINDOWS"
	
	filter "configurations:Debug"
		defines "PHX_DEBUG"
		symbols "On"
		
		links
		{
			"%{TANG_Libraries.glfw_debug}",
		}
	
	filter "configurations:Release"
		defines "PHX_RELEASE"
		optimize "On"
		
		links
		{
			"%{TANG_Libraries.glfw_release}",
		}