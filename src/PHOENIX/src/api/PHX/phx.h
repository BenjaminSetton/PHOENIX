#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/key_codes.h"
#include "PHX/types/metrics.h"
#include "PHX/types/settings.h"
#include "PHX/types/shader_desc.h"
#include "PHX/types/status_code.h"

#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/window.h"

namespace PHX
{
	// State calls
	PHX_API STATUS_CODE Initialize(const Settings& initSettings, WindowHandle window);
	PHX_API STATUS_CODE Update(float deltaTime);
	PHX_API STATUS_CODE Shutdown();

	// Returns the combined PHX library versions into a single u32
	PHX_API u32 GetFullVersion();

	// Returns the individual PHX library version components
	PHX_API u32 GetMajorVersion();
	PHX_API u32 GetMinorVersion();
	PHX_API u32 GetPatchVersion();

	PHX_API STATUS_CODE CreateWindow(const WindowCreateInfo& createInfo, WindowHandle& window);
	PHX_API STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, RenderDeviceHandle& renderDevice);

	// UTILS
	// TODO - Support multi-stage compilation
	PHX_API STATUS_CODE CompileShader(const ShaderSourceData& srcData, CompiledShader& out_result);
}