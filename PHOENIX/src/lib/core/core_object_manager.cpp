
#include "core_object_manager.h"

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
		const HANDLE_TYPE type = handle.GetType();
		switch (type)
		{
		case HANDLE_TYPE::RENDER_DEVICE: return m_renderDevices.Resolve(handle.GetIndex());
		case HANDLE_TYPE::WINDOW:        return m_windows.Resolve(handle.GetIndex());
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
		const HANDLE_TYPE type = handle.GetType();
		switch (type)
		{
		case HANDLE_TYPE::RENDER_DEVICE: m_renderDevices.IncrementRefCount(handle.GetIndex()); break;
		case HANDLE_TYPE::WINDOW:        m_windows.IncrementRefCount(handle.GetIndex());       break;
		default:
		{
			ASSERT_ALWAYS("Failed to increment handle ref count. Unhandled type!");
			break;
		}
		}
	}

	void CoreObjectManager::DecrementHandleRefCount(const Handle& handle)
	{
		const HANDLE_TYPE type = handle.GetType();
		switch (type)
		{
		case HANDLE_TYPE::RENDER_DEVICE: m_renderDevices.DecrementRefCount(handle.GetIndex()); break;
		case HANDLE_TYPE::WINDOW:        m_windows.DecrementRefCount(handle.GetIndex());       break;
		default:
		{
			ASSERT_ALWAYS("Failed to increment handle ref count. Unhandled type!");
			break;
		}
		}
	}

	STATUS_CODE CoreObjectManager::CreateCoreObjects(WindowHandle window)
	{
		auto& settings = GetSettings();
		switch (settings.backendAPI)
		{
		case GRAPHICS_API::VULKAN:
		{
			return CoreVk::Get().Initialize(window);
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

	STATUS_CODE CoreObjectManager::CreateWindow(const WindowCreateInfo& createInfo, WindowHandle& window)
	{
#if defined(PHX_WINDOWS)
		WindowWin64* pWindow = new WindowWin64(createInfo);
		return HANDLE_UTILS::AllocateHandle<IWindow>(m_windows, pWindow, this, window);
#else
#		error Unsupported platform!
#endif
	}

	STATUS_CODE CoreObjectManager::CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, RenderDeviceHandle& renderDevice)
	{
		auto& settings = GetSettings();
		switch (settings.backendAPI)
		{
		case GRAPHICS_API::VULKAN:
		{
			const u32 numRenderDevices = m_renderDevices.Size();
			if (numRenderDevices > 0)
			{
				LogWarning("Cannot re-create render device. An instance already exists!");
				return STATUS_CODE::SUCCESS;
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