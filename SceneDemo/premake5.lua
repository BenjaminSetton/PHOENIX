
include "vendor/dependencies.lua"

outputDir = "%{cfg.system}/%{cfg.buildcfg}/%{cfg.architecture}"

project "SceneDemo"
	location "out"
	language "C++"
	kind "ConsoleApp"
	
	targetdir ("out/bin/" .. outputDir)
	objdir ("out/obj/" .. outputDir)
	
	dependson
	{
		"PHOENIX"
	}
	
	files
	{
		"src/**.h",
		"src/**.cpp"
	}
	
	includedirs
	{
		"%{SceneDemo_IncludeDirs.PHOENIX}",
	}
	
	filter "system:windows"
		cppdialect "C++14"
		systemversion "latest"
		warnings "High"
	
		filter "configurations:Debug"
			symbols "On"
			
			links
			{
				"%{SceneDemo_Libraries.PHOENIX_win64_debug}",
			}
		
		filter "configurations:Release"
			optimize "On"
			
			links
			{
				"%{SceneDemo_Libraries.PHOENIX_win64_release}",
			}