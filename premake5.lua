local function GetArchitecture()
	-- Get the host architecture from a custom PHX_ARCH that's explicitly set by the user.
	-- The premake5 binary uses x86_64, so we're not able to distinguish Windows-on-ARM, since
	-- it returns AMD64 in all cases. x86_64 emulation in WoA makes it very hard to get processor architecture
	local hostArch = os.getenv("PHX_ARCH")
	print("Using architecture \"" .. hostArch .. "\"")
	return hostArch
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