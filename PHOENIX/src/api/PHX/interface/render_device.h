#pragma once

#include "PHX/types/status_code.h"

#include "PHX/interface/buffer.h"
#include "PHX/interface/device_context.h"
#include "PHX/interface/pipeline.h"
#include "PHX/interface/render_graph.h"
#include "PHX/interface/shader.h"
#include "PHX/interface/texture.h"
#include "PHX/interface/uniform.h"
#include "PHX/interface/window.h"

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
		virtual STATUS_CODE AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, IDeviceContext** out_deviceContext)																					= 0; // TODO - REMOVE
		virtual STATUS_CODE AllocateRenderGraph(IRenderGraph** out_renderGraph)																																		= 0;
		virtual STATUS_CODE AllocateBuffer(const BufferCreateInfo& createInfo, IBuffer** out_buffer)																												= 0;
		virtual STATUS_CODE AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, ITexture** out_texture)	= 0;
		virtual STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, IShader** out_shader)																												= 0;
		virtual STATUS_CODE AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, IUniformCollection** out_uniformCollection)																	= 0;

		// Deallocations
		virtual void DeallocateDeviceContext(IDeviceContext** pDeviceContext)				= 0;
		virtual void DeallocateBuffer(IBuffer** pBuffer)									= 0;
		virtual void DeallocateTexture(ITexture** pTexture)									= 0;
		virtual void DeallocateShader(IShader** pShader)									= 0;
		virtual void DeallocateUniformCollection(IUniformCollection** pUniformCollection)	= 0;
	};
}
