
include "vendor/dependencies.lua"

project "PHOENIX"
	location "out/PHX"
	language "C++"
	kind "StaticLib"
	
	targetdir ("out/PHX/bin/" .. outputDir)
	objdir ("out/PHX/obj/" .. outputDir)
	
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
		"%{PHX_IncludeDirs.dep_glslang}",
		
		"%{PHX_IncludeDirs.inc_api}",
		"%{PHX_IncludeDirs.inc_lib}",
	}
	
	links
	{
		"%{PHX_Libraries.vulkan}"
	}
	
	filter "system:windows"
		cppdialect "C++14"
		systemversion "latest"
		warnings "High"
		defines "PHX_WINDOWS"
		
		filter "configurations:Debug"
			links
			{
				"%{PHX_Libraries.glslang_os_dependent_win_debug}"
			}
			
		filter "configurations:Release"
			links
			{
				"%{PHX_Libraries.glslang_os_dependent_win_release}"
			}
	
	-- Platform-independent configurations
	filter "configurations:Debug"
		defines "PHX_DEBUG"
		symbols "On"
		
		links
		{
			"%{PHX_Libraries.glfw_debug}",
			"%{PHX_Libraries.glslang_debug}",
			"%{PHX_Libraries.SPV_debug}",
			"%{PHX_Libraries.SPV_tools_debug}",
			"%{PHX_Libraries.SPV_tools_opt_debug}",
			"%{PHX_Libraries.glslang_code_gen_debug}",
			"%{PHX_Libraries.glslang_machine_independent_debug}"
		}
	
	filter "configurations:Release"
		defines "PHX_RELEASE"
		optimize "On"
		
		links
		{
			"%{PHX_Libraries.glfw_release}",
			"%{PHX_Libraries.glslang_release}",
			"%{PHX_Libraries.SPV_release}",
			"%{PHX_Libraries.SPV_tools_release}",
			"%{PHX_Libraries.SPV_tools_opt_release}",
			"%{PHX_Libraries.glslang_code_gen_release}",
			"%{PHX_Libraries.glslang_machine_independent_release}"
		}
		