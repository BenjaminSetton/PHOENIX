
#include "object_factory.h"

#include "platform/vulkan/core_vk.h"
#include "platform/vulkan/render_device_vk.h"
#include "platform/vulkan/swap_chain_vk.h"

#include "core/global_settings.h"
#include "core/handle/handle_utils.h"
#include "utils/logger.h"
#include "utils/sanity.h"

#if defined(PHX_WINDOWS)
#include "../platform/win64/window_win64.h"
#else
#error Invalid platform!
#endif

namespace PHX
{
	void* CoreObjectManager::ResolveHandle(const Handle& handle)
	{
		const HANDLE_TYPE type = HandleAccessor::GetType(handle);
		switch (type)
		{
		case HANDLE_TYPE::RENDER_DEVICE: return HANDLE_UTILS::ResolveHandle<IRenderDevice>(m_renderDevices, handle);
		default:
		{
			break;
		}
		}

		ASSERT_ALWAYS("Failed to resolve handle. Unhandled type!");
		return nullptr;
	}

	void CoreObjectManager::IncrementHandleRefCount(const Handle& handle)
	{
		const HANDLE_TYPE type = HandleAccessor::GetType(handle);
		switch (type)
		{
		case HANDLE_TYPE::RENDER_DEVICE: 
		{
			HANDLE_UTILS::IncrementRefCount<RenderDeviceHandle, IRenderDevice>(handle);
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to increment handle ref count. Unhandled type!");
			break;
		}
		}
	}

	void CoreObjectManager::DecrementHandleRefCount(const Handle& handle)
	{
		const HANDLE_TYPE type = HandleAccessor::GetType(handle);
		switch (type)
		{
		case HANDLE_TYPE::RENDER_DEVICE:
		{
			HANDLE_UTILS::DecrementRefCount<RenderDeviceHandle, IRenderDevice>(handle, m_renderDevices);
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to increment handle ref count. Unhandled type!");
			break;
		}
		}
	}

	STATUS_CODE CoreObjectManager::CreateCoreObjects(IWindow* pWindow)
	{
		auto& settings = GetSettings();
		switch (settings.backendAPI)
		{
		case GRAPHICS_API::VULKAN:
		{
			return CoreVk::Get().Initialize(pWindow);
		}
		default:
		{
			ASSERT_ALWAYS("Failed to create core objects. Unsupported graphics API!");
			break;
		}
		}

		// Nothing was created
		return STATUS_CODE::ERR_INTERNAL;
	}

	IWindow* CoreObjectManager::CreateWindow(const WindowCreateInfo& createInfo)
	{
#if defined(PHX_WINDOWS)
		return new WindowWin64(createInfo);
#else
#    error Invalid platform!
#endif
	}

	STATUS_CODE CoreObjectManager::CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, RenderDeviceHandle& renderDevice)
	{
		auto& settings = GetSettings();
		switch (settings.backendAPI)
		{
		case GRAPHICS_API::VULKAN:
		{
			const u32 numRenderDevices = static_cast<u32>(m_renderDevices.size());
			if (numRenderDevices > 0)
			{
				LogWarning("Cannot re-create render device. An instance already exists!");
				return STATUS_CODE::ERR_API;
			}
			else
			{
				RenderDeviceVk* pRenderDevice = new RenderDeviceVk(createInfo);
				if (pRenderDevice == nullptr)
				{
					LogError("Failed to create render device. Memory allocation failed!");
					return STATUS_CODE::ERR_INTERNAL;
				}
				
				return HANDLE_UTILS::AllocateHandle<IRenderDevice>(m_renderDevices, pRenderDevice, this, renderDevice);
			}
		}
		default:
		{
			ASSERT_ALWAYS("Failed to create render device. Unsupported graphics API!");
			break;
		}
		}

		return STATUS_CODE::ERR_API;
	}
}