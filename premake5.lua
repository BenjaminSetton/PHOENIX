local function GetArchitecture()
	local hostArch = os.hostarch()
	local arch
	if hostArch == "x86_64" or hostArch == "x86" then
		arch = "x64"
	elseif hostArch == "arm64" or hostArch == "aarch64" then
		arch = "ARM64"
	else
		error("Unsupported host architecture: " .. tostring(hostArch), 0)
	end
	print("Detected host architecture \"" .. hostArch .. "\", building for \"" .. arch .. "\"")
	return arch
end

workspace "PHOENIX"

	architecture(GetArchitecture())
	startproject "ImGui"

	-- Enable multi-threaded builds
	multiprocessorcompile "On"

	configurations
	{
		"Debug",
		"Release",
		"Sanitizer"
	}
	
configLower = { Debug = "debug", Release = "release", Sanitizer = "sanitizer" }
slangConfigLower = { Debug = "debug", Release = "release", Sanitizer = "debug" }
ConfigMap = {
    debugSuffix = { Debug = "d", Release = "", Sanitizer = "d" }
}

outputDir = "%{cfg.system}/%{configLower[cfg.buildcfg]}/%{cfg.architecture}"

-- Sub-project root paths relative to the workspace directory
PHX_ROOT     = "src/PHOENIX"
SAMPLES_ROOT = "src/samples"
BSL_ROOT     = "src/BSL"

-- Core lib
include(PHX_ROOT .. "/premake5.lua")

-- Samples
group "Samples"
include(SAMPLES_ROOT .. "/HelloTriangle/premake5.lua")
include(SAMPLES_ROOT .. "/RayTracing/premake5.lua")
include(SAMPLES_ROOT .. "/BasicModel/premake5.lua")
include(SAMPLES_ROOT .. "/TexturedModel/premake5.lua")
include(SAMPLES_ROOT .. "/ComputeParticles/premake5.lua")
include(SAMPLES_ROOT .. "/ImGui/premake5.lua")
include(SAMPLES_ROOT .. "/InstancedAnimation/premake5.lua")
include(SAMPLES_ROOT .. "/Tessellation/premake5.lua")
include(SAMPLES_ROOT .. "/Lod/premake5.lua")
group ""