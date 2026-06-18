#pragma once

#include "PHX/types/status_code.h"

#include "PHX/interface/buffer.h"
#include "PHX/interface/device_context.h"
#include "PHX/interface/handle_owner.h"
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

	struct RenderDeviceHandle : Handle
	{
		DECLARE_HANDLE(RenderDeviceHandle)

		const char* GetDeviceName() const;
		u32 GetFramesInFlight() const;

		// Allocations
		STATUS_CODE AllocateBuffer(const BufferCreateInfo& createInfo, BufferHandle& buffer);
		STATUS_CODE AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, TextureHandle& texture);
		STATUS_CODE AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, UniformCollectionHandle& uniformCollection);
		STATUS_CODE AllocateRenderGraph(RenderGraphHandle& renderGraph);
		STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, ShaderHandle& shader);
		STATUS_CODE AllocateSwapChain(const SwapChainCreateInfo& createInfo, SwapChainHandle& swapChain);
		STATUS_CODE AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, DeviceContextHandle& deviceContext);
	};

	class IRenderDevice : public RefCounted, public HandleOwner
	{
	public:

		virtual ~IRenderDevice() { }

		// Query device stats
		virtual const char* GetDeviceName() const = 0;
		virtual u32 GetFramesInFlight() const = 0;

		// Allocations
		virtual STATUS_CODE AllocateBuffer(const BufferCreateInfo& createInfo, BufferHandle& handle) = 0;
		virtual STATUS_CODE AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, TextureHandle& handle) = 0;
		virtual STATUS_CODE AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, UniformCollectionHandle& uniformCollection) = 0;
		virtual STATUS_CODE AllocateRenderGraph(RenderGraphHandle& renderGraph) = 0;
		virtual STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, ShaderHandle& shader) = 0;
		virtual STATUS_CODE AllocateSwapChain(const SwapChainCreateInfo& createInfo, SwapChainHandle& swapChain) = 0;
		virtual STATUS_CODE AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, DeviceContextHandle& deviceContext) = 0;
	};
}
