
include "../common/vendor/dependencies.lua"

project "Lod"
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
		"src/shaders/**",
		"../common/src/**.h",
		"../common/src/**.cpp",
		"../common/src/shaders/**",
		"../common/vendor/imgui/imgui.cpp",
		"../common/vendor/imgui/imgui_draw.cpp",
		"../common/vendor/imgui/imgui_tables.cpp",
		"../common/vendor/imgui/imgui_widgets.cpp",
		"../common/vendor/imgui/imgui_demo.cpp",
	}

	includedirs
	{
		"%{SamplesCommon_IncludeDirs.PHOENIX}",
		"%{SamplesCommon_IncludeDirs.glm}",
		"%{SamplesCommon_IncludeDirs.assimp_core}",
		"%{SamplesCommon_IncludeDirs.assimp_generated}",
		"%{SamplesCommon_IncludeDirs.stb_image}",
		"%{SamplesCommon_IncludeDirs.imgui}"
	}

	links
	{
		"%{SamplesCommon_Libraries.PHOENIX}",
		"%{SamplesCommon_Libraries.assimp_zlib}",
		"%{SamplesCommon_Libraries.assimp}",
		"%{SamplesCommon_Libraries.glm}"
	}

	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"
		warnings "Extra"

	filter "configurations:Debug"
		symbols "On"

	filter "configurations:Release"
		optimize "On"

	filter "configurations:Sanitizer"
		symbols "On"
		editandcontinue "Off"

	filter { "system:windows", "configurations:Sanitizer" }
		buildoptions { "/fsanitize=address" }
		linkoptions { "/INCREMENTAL:NO" }

	filter {}
		SamplesCommon_SetAssetDefines()
		SamplesCommon_CopyDLLs()
