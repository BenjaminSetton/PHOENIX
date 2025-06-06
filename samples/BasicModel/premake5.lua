
include "../common/vendor/dependencies.lua"

project "BasicModel"
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
		"src/**.cpp",
		"src/**.vert",
		"src/**.frag"
	}
	
	includedirs
	{
		"%{SamplesCommon_IncludeDirs.PHOENIX}",
	}
	
	filter "system:windows"
		cppdialect "C++14"
		systemversion "latest"
		warnings "High"
	
		filter "configurations:Debug"
			symbols "On"
			
			links
			{
				"%{SamplesCommon_Libraries.PHOENIX_win64_debug}",
			}
		
		filter "configurations:Release"
			optimize "On"
			
			links
			{
				"%{SamplesCommon_Libraries.PHOENIX_win64_release}",
			}