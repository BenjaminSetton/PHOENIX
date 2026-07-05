#pragma once

#include "PHX/interface/buffer.h"
#include "PHX/interface/device_context.h"
#include "PHX/interface/handle_owner.h"
#include "PHX/interface/render_graph.h"
#include "PHX/interface/shader.h"
#include "PHX/interface/texture.h"
#include "PHX/interface/uniform.h"
#include "PHX/interface/window.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	typedef void(*DebugMessageCallbackFn)(const char* msg);

	struct RenderDeviceCreateInfo
	{
		DebugMessageCallbackFn debugMessageCallback = nullptr;
		WindowHandle window							= INVALID_HANDLE; // Currently unused, but keeping around for possible future multi-window support
		u32 framesInFlight							= 2;
	};

	struct RenderDeviceHandle : Handle
	{
		DECLARE_PHX_HANDLE(RenderDeviceHandle);

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
}
