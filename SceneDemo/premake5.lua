
outputDir = "%{cfg.system}/%{cfg.buildcfg}/%{cfg.architecture}"

project "SceneDemo"
	location "out"
	language "C++"
	kind "ConsoleApp"
	
	targetdir ("out/bin/" .. outputDir)
	objdir ("out/obj/" .. outputDir)
	
	files
	{
		"src/**.h",
		"src/**.cpp"
	}
	
	filter "system:windows"
		cppdialect "C++14"
		systemversion "latest"
		warnings "High"
	
	filter "configurations:Debug"
		symbols "On"
	
	filter "configurations:Release"
		optimize "On"