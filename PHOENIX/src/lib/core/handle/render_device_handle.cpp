
#include "PHX/interface/render_device.h"

#include "core/handle/handle_utils.h"
#include "core/interface_types/render_device_interface.h"
#include "utils/sanity.h"

namespace PHX
{
	static const ShaderReflectionData s_defaultReflectionData;

	DEFINE_PHX_HANDLE(RenderDeviceHandle, HANDLE_TYPE::RENDER_DEVICE)

	const char* RenderDeviceHandle::GetDeviceName() const
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->GetDeviceName();
		}

		ASSERT_ALWAYS("Failed to get device name. Could not resolve render device handle!");
		return nullptr;
	}

	u32 RenderDeviceHandle::GetFramesInFlight() const
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->GetFramesInFlight();
		}

		ASSERT_ALWAYS("Failed to get frames in flight. Could not resolve render device handle!");
		return 0;
	}

	bool RenderDeviceHandle::IsRayTracingSupported() const
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->IsRayTracingSupported();
		}

		ASSERT_ALWAYS("Failed to query ray tracing support. Could not resolve render device handle!");
		return false;
	}

	STATUS_CODE RenderDeviceHandle::AllocateBuffer(const BufferCreateInfo& createInfo, BufferHandle& buffer)
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->AllocateBuffer(createInfo, buffer);
		}

		ASSERT_ALWAYS("Failed to allocate buffer. Could not resolve render device handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderDeviceHandle::AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, TextureHandle& texture)
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->AllocateTexture(baseCreateInfo, viewCreateInfo, samplerCreateInfo, texture);
		}

		ASSERT_ALWAYS("Failed to allocate texture. Could not resolve render device handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderDeviceHandle::AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, UniformCollectionHandle& uniformCollection)
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->AllocateUniformCollection(createInfo, uniformCollection);
		}

		ASSERT_ALWAYS("Failed to allocate uniform collection. Could not resolve render device handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderDeviceHandle::AllocateRenderGraph(RenderGraphHandle& renderGraph)
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->AllocateRenderGraph(renderGraph);
		}

		ASSERT_ALWAYS("Failed to allocate render graph. Could not resolve render device handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderDeviceHandle::AllocateShader(const ShaderCreateInfo& createInfo, ShaderHandle& shader)
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->AllocateShader(createInfo, shader);
		}

		ASSERT_ALWAYS("Failed to allocate shader. Could not resolve render device handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderDeviceHandle::AllocateSwapChain(const SwapChainCreateInfo& createInfo, SwapChainHandle& swapChain)
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->AllocateSwapChain(createInfo, swapChain);
		}

		ASSERT_ALWAYS("Failed to allocate swap chain. Could not resolve render device handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderDeviceHandle::AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, DeviceContextHandle& deviceContext)
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->AllocateDeviceContext(createInfo, deviceContext);
		}

		ASSERT_ALWAYS("Failed to allocate device context. Could not resolve render device handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderDeviceHandle::AllocateAccelerationStructure(const AccelerationStructureCreateInfo& createInfo, AccelerationStructureHandle& accelerationStructure)
	{
		IRenderDevice* pDevice = HANDLE_UTILS::ResolveHandle(*this);
		if (pDevice != nullptr)
		{
			return pDevice->AllocateAccelerationStructure(createInfo, accelerationStructure);
		}

		ASSERT_ALWAYS("Failed to allocate acceleration structure. Could not resolve render device handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}
}