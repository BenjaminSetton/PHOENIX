#pragma once

#include <memory>
#include <vector>

#include "BSL/deferred_caller.h"
#include "core/handle/handle_list.h"
#include "core/handle/handle_owner.h"
#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/window.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	// Forward declarations
	class IRenderDevice;
	class IWindow;

	class CoreObjectManager : public HandleOwner
	{
	public:

		static CoreObjectManager& Get()
		{
			static CoreObjectManager s_instance;
			return s_instance;
		}

		void* ResolveHandle(const Handle& handle) override;
		void IncrementHandleRefCount(const Handle& handle) override;
		void DecrementHandleRefCount(const Handle& handle) override;

		STATUS_CODE CreateCoreObjects(WindowHandle window);
		STATUS_CODE CreateWindow(const WindowCreateInfo& createInfo, WindowHandle& window);
		STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, RenderDeviceHandle& renderDevice);

		BSL::DeferredCaller& GetDeferredCaller();
		const BSL::DeferredCaller& GetDeferredCaller() const;

		// Waits for GPU to be idle so resources can be cleaned up properly
		STATUS_CODE Shutdown();

	private:
		HandleList<IWindow> m_windows;
		HandleList<IRenderDevice> m_renderDevices;

		BSL::DeferredCaller m_deferredCaller;
	};
}