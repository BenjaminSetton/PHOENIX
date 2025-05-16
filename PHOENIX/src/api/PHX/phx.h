#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/settings.h"
#include "PHX/types/shader_desc.h"
#include "PHX/types/status_code.h"

#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/window.h"

namespace PHX
{
	// INIT
	STATUS_CODE Initialize(const Settings& initSettings, IWindow* pWindow);

	STATUS_CODE CreateWindow(const WindowCreateInfo& createInfo, IWindow** out_window);
	STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, IRenderDevice** out_renderDevice);

	void DestroyWindow(IWindow** pWindow);
	void DestroyRenderDevice(IRenderDevice** pRenderDevice);

	// UTILS
	STATUS_CODE CompileShader(const ShaderSourceData& srcData, CompiledShader& out_result);
}