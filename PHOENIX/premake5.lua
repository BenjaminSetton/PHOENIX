
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
		"src/**.cpp",
		"src/**.inl"
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
		"%{PHX_Libraries.vulkan}",
		"%{PHX_Libraries.glfw}",
		"%{PHX_Libraries.glslang}",
		"%{PHX_Libraries.SPV}",
		"%{PHX_Libraries.glslang_code_gen}",
		"%{PHX_Libraries.glslang_machine_independent}"
	}

	filter "system:windows"
		cppdialect "C++14"
		systemversion "latest"
		warnings "Extra"
		defines "PHX_WINDOWS"
		links
		{
			"%{PHX_Libraries.glslang_os_dependent_win}"
		}

	filter "configurations:Debug"
		defines "PHX_DEBUG"
		symbols "On"

	filter "configurations:Release"
		defines "PHX_RELEASE"
		optimize "On"

	filter "configurations:Sanitizer"
		defines { "PHX_DEBUG", "PHX_SANITIZE" }
		symbols "On"
		editandcontinue "Off"

	filter { "system:windows", "configurations:Sanitizer" }
		buildoptions { "/fsanitize=address" }
