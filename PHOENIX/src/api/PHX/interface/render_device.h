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
		u32 framesInFlight							= 2;
	};

	// TODO - Create handle

	class IRenderDevice
	{
	public:

		virtual ~IRenderDevice() { }

		// Query device stats
		virtual const char* GetDeviceName() const = 0;

		// Allocations
		virtual STATUS_CODE AllocateSwapChain(const SwapChainCreateInfo& createInfo, ISwapChain** out_swapChain)																									= 0;
		virtual STATUS_CODE AllocateRenderGraph(IRenderGraph** out_renderGraph)																																		= 0;
		virtual STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, IShader** out_shader)																												= 0;
		
		virtual STATUS_CODE AllocateBuffer(const BufferCreateInfo& createInfo, BufferHandle& buffer)																												= 0;
		virtual STATUS_CODE AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, TextureHandle& texture)	= 0;
		virtual STATUS_CODE AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, UniformCollectionHandle& uniformCollection)																	= 0;

		// Deallocations
		virtual void DeallocateSwapChain(ISwapChain** out_swapChain)						= 0;
		virtual void DeallocateRenderGraph(IRenderGraph** pRenderGraph)						= 0;
		virtual void DeallocateShader(IShader** pShader)									= 0; 

		virtual void DeallocateResource(const Handle& handle)								= 0;

		// LIB-ONLY FUNCTIONS BELOW - THIS WILL BE SPLIT OUT PROPERLY ONCE THE HANDLE FOR THE RENDER DEVICE IS CREATED

		// Getters
		virtual u32 GetFramesInFlight() const = 0;

		virtual STATUS_CODE AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, DeviceContextHandle& deviceContext)																					= 0;

		// Handles
		virtual ITexture* ResolveHandle(const TextureHandle& handle) = 0;
		virtual IBuffer* ResolveHandle(const BufferHandle& handle) = 0;
		virtual IUniformCollection* ResolveHandle(const UniformCollectionHandle& handle) = 0;
		virtual IDeviceContext* ResolveHandle(const DeviceContextHandle& handle) = 0;

		virtual void IncrementRefCount(const Handle& handle) = 0;
		virtual void DecrementRefCount(const Handle& handle) = 0;
	};
}
