#pragma once

#include "../types/status_code.h"
#include "framebuffer.h"
#include "pipeline.h"
#include "shader.h"
#include "window.h"

namespace PHX
{
	typedef void(*DebugMessageCallbackFn)(const char* msg);

	struct RenderDeviceCreateInfo
	{
		DebugMessageCallbackFn debugMessageCallback = nullptr;
		IWindow* window								= nullptr;
	};

	class IRenderDevice
	{
	public:

		virtual ~IRenderDevice() { }

		// Query device stats
		virtual const char* GetDeviceName() const = 0;

		// Allocations
		virtual STATUS_CODE AllocateBuffer()                                             = 0;
		virtual STATUS_CODE AllocateFramebuffer(const FramebufferCreateInfo& createInfo, IFramebuffer** out_framebuffer) = 0;
		virtual STATUS_CODE AllocateCommandBuffer()                                      = 0;
		virtual STATUS_CODE AllocateTexture()                                            = 0;
		virtual STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, IShader** out_shader)                     = 0;
		virtual STATUS_CODE AllocatePipeline(const PipelineCreateInfo& createInfo, IPipeline** out_pipeline)             = 0;

		// Deallocations
		virtual void DeallocateFramebuffer(IFramebuffer* pFramebuffer) = 0;
		virtual void DeallocateTexture(ITexture* pTexture) = 0;
		virtual void DeallocateShader(IShader* pShader) = 0;
		virtual void DeallocatePipeline(IPipeline* pPipeline) = 0;
	};
}
