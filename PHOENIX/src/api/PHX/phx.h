#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/key_codes.h"
#include "PHX/types/settings.h"
#include "PHX/types/shader_desc.h"
#include "PHX/types/status_code.h"

#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/window.h"

namespace PHX
{
	// INIT
	STATUS_CODE Initialize(const Settings& initSettings, WindowHandle window);

	// Returns the combined versions into a single u32
	u32 GetFullVersion();

	// Returns the individual version components
	u32 GetMajorVersion();
	u32 GetMinorVersion();
	u32 GetPatchVersion();

	STATUS_CODE CreateWindow(const WindowCreateInfo& createInfo, WindowHandle& window);
	STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, RenderDeviceHandle& renderDevice);

	// UTILS
	STATUS_CODE CompileShader(const ShaderSourceData& srcData, CompiledShader& out_result);
}