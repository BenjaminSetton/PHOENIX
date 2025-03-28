#pragma once

#include "PHX/types/status_code.h"

#include "buffer.h"
#include "device_context.h"
#include "framebuffer.h"
#include "pipeline.h"
#include "shader.h"
#include "texture.h"
#include "uniform.h"
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
		virtual STATUS_CODE AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, IDeviceContext** out_deviceContext)																					= 0;
		virtual STATUS_CODE AllocateBuffer(const BufferCreateInfo& createInfo, IBuffer** out_buffer)																												= 0;
		virtual STATUS_CODE AllocateFramebuffer(const FramebufferCreateInfo& createInfo, IFramebuffer** out_framebuffer)																							= 0;
		virtual STATUS_CODE AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, ITexture** out_texture)	= 0;
		virtual STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, IShader** out_shader)																												= 0;
		virtual STATUS_CODE AllocateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo, IPipeline** out_pipeline)																						= 0;
		virtual STATUS_CODE AllocateComputePipeline(const ComputePipelineCreateInfo& createInfo, IPipeline** out_pipeline)																							= 0;
		virtual STATUS_CODE AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, IUniformCollection** out_uniformCollection)																	= 0;

		// Deallocations
		virtual void DeallocateDeviceContext(IDeviceContext** pDeviceContext)				= 0;
		virtual void DeallocateBuffer(IBuffer** pBuffer)									= 0;
		virtual void DeallocateFramebuffer(IFramebuffer** pFramebuffer)						= 0;
		virtual void DeallocateTexture(ITexture** pTexture)									= 0;
		virtual void DeallocateShader(IShader** pShader)									= 0;
		virtual void DeallocatePipeline(IPipeline** pPipeline)								= 0;
		virtual void DeallocateUniformCollection(IUniformCollection** pUniformCollection)	= 0;
	};
}
