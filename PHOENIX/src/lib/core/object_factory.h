#pragma once

#include <memory>
#include <vector>

#include "PHX/interface/handle_owner.h"
#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/window.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"

namespace PHX
{
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

		STATUS_CODE CreateCoreObjects(IWindow* pWindow);
		IWindow* CreateWindow(const WindowCreateInfo& createInfo);
		STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, RenderDeviceHandle& handle);

	private:
		std::vector<IWindow*> m_windows;
		std::vector<IRenderDevice*> m_renderDevices;
	};
}