
include "../common/vendor/dependencies.lua"

project "TexturedModel"
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
		"src/**.frag",
		"../common/src/**.h",
		"../common/src/**.cpp"
	}
	
	includedirs
	{
		"%{SamplesCommon_IncludeDirs.PHOENIX}",
		"%{SamplesCommon_IncludeDirs.assimp_core}",
		"%{SamplesCommon_IncludeDirs.assimp_generated}",
		"%{SamplesCommon_IncludeDirs.glm}",
		"%{SamplesCommon_IncludeDirs.stb_image}"
	}
	
	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"
		warnings "High"
	
		filter "configurations:Debug"
			symbols "On"
			
			links
			{
				"%{SamplesCommon_Libraries.PHOENIX_win64_debug}",
				"%{SamplesCommon_Libraries.assimp_zlib_debug}",
				"%{SamplesCommon_Libraries.assimp_debug}",
				"%{SamplesCommon_Libraries.glm_debug}"
			}
		
		filter "configurations:Release"
			optimize "On"
			
			links
			{
				"%{SamplesCommon_Libraries.PHOENIX_win64_release}",
				"%{SamplesCommon_Libraries.assimp_zlib_release}",
				"%{SamplesCommon_Libraries.assimp_release}",
				"%{SamplesCommon_Libraries.glm_release}"
			}