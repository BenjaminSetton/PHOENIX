#pragma once

#include "core/handle/handle_owner.h"
#include "core/ref.h"
#include "PHX/interface/acceleration_structure.h"
#include "PHX/interface/buffer.h"
#include "PHX/interface/device_context.h"
#include "PHX/interface/render_graph.h"
#include "PHX/interface/shader.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/texture.h"
#include "PHX/interface/uniform.h"

namespace PHX
{
	class IRenderDevice : public RefCounted, public HandleOwner
	{
	public:

		virtual ~IRenderDevice() { }

		// Query device stats
		virtual const char* GetDeviceName() const = 0;
		virtual u32 GetFramesInFlight() const = 0;
		virtual bool IsRayTracingSupported() const = 0;

		// Allocations
		virtual STATUS_CODE AllocateBuffer(const BufferCreateInfo& createInfo, BufferHandle& handle) = 0;
		virtual STATUS_CODE AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, TextureHandle& handle) = 0;
		virtual STATUS_CODE AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, UniformCollectionHandle& uniformCollection) = 0;
		virtual STATUS_CODE AllocateRenderGraph(RenderGraphHandle& renderGraph) = 0;
		virtual STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, ShaderHandle& shader) = 0;
		virtual STATUS_CODE AllocateSwapChain(const SwapChainCreateInfo& createInfo, SwapChainHandle& swapChain) = 0;
		virtual STATUS_CODE AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, DeviceContextHandle& deviceContext) = 0;
		virtual STATUS_CODE AllocateAccelerationStructure(const AccelerationStructureCreateInfo& createInfo, AccelerationStructureHandle& handle) = 0;
	};
}