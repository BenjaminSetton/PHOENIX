
include "vendor/dependencies.lua"

project "PHOENIX"
	location "out"
	language "C++"
	kind "StaticLib"
	
	targetdir ("out/bin/" .. outputDir)
	objdir ("out/obj/" .. outputDir)
	
	dependson
	{
		"GLFW"
	}
	
	files
	{
		"src/**.h",
		"src/**.cpp"
	}
	
	-- For library includes, I'll list the most specific include directories first. Not sure if
	-- the preprocessor looks for files through the include_dirs in the same order that
	-- they're declared, but it doesn't hurt to organize them this way I guess
	includedirs
	{
		"%{PHX_IncludeDirs.dep_vulkan}",
		"%{PHX_IncludeDirs.dep_glfw}",
		"%{PHX_IncludeDirs.dep_vma}",
		"%{PHX_IncludeDirs.dep_shaderc}",
		
		"%{PHX_IncludeDirs.lib_inc}",
	}
	
	links
	{
		"%{PHX_Libraries.vulkan}",
		"%{PHX_Libraries.glfw}"
	}
	
	filter "system:windows"
		cppdialect "C++14"
		systemversion "latest"
		warnings "High"
		defines "PHX_WINDOWS"
	
	filter "configurations:Debug"
		defines "PHX_DEBUG"
		symbols "On"
		
		--links
		--{
		--	"%{PHX_Libraries.glfw_debug}",
		--	"%{PHX_Libraries.shaderc_debug}"
		--}
	
	filter "configurations:Release"
		defines "PHX_RELEASE"
		optimize "On"
		
		--links
		--{
		--	"%{PHX_Libraries.glfw_release}",
		--	"%{PHX_Libraries.shaderc_release}",
		--}