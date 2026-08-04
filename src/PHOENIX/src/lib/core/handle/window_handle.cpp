
#include <stdarg.h>

#include "BSL/logger.h"
#include "core/handle/handle_utils.h"
#include "core/interface_types/window_interface.h"
#include "PHX/interface/render_device.h"
#include "PHX/interface/window.h"

using namespace BSL;

namespace PHX
{
	DEFINE_PHX_HANDLE(WindowHandle, HANDLE_TYPE::WINDOW)

	u32 WindowHandle::GetCurrentWidth() const
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->GetCurrentWidth();
		}

		LogError("Failed to get current width. Could not resolve window handle!");
		return 0;
	}

	u32 WindowHandle::GetCurrentHeight() const
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->GetCurrentHeight();
		}

		LogError("Failed to get current height. Could not resolve window handle!");
		return 0;
	}

	int WindowHandle::GetPositionX() const
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->GetPositionX();
		}

		LogError("Failed to get positionX. Could not resolve window handle!");
		return 0;
	}

	int WindowHandle::GetPositionY() const
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->GetPositionY();
		}

		LogError("Failed to get positionY. Could not resolve window handle!");
		return 0;
	}

	const char* WindowHandle::GetName() const
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->GetName();
		}

		LogError("Failed to get name. Could not resolve window handle!");
		return nullptr;
	}

	void WindowHandle::Update(float deltaTime)
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->Update(deltaTime);
		}

		LogError("Failed to update. Could not resolve window handle!");
	}

	bool WindowHandle::InFocus() const
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->InFocus();
		}

		LogError("Failed to get focus. Could not resolve window handle!");
		return false;
	}

	bool WindowHandle::ShouldClose() const
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->ShouldClose();
		}

		LogError("Failed to get should close. Could not resolve window handle!");
		return true;
	}

	bool WindowHandle::IsMinimized() const
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->IsMinimized();
		}

		LogError("Failed to get minimized. Could not resolve window handle!");
		return false;
	}

	bool WindowHandle::IsMaximized() const
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			return pWindow->IsMaximized();
		}

		LogError("Failed to get is maximized. Could not resolve window handle!");
		return false;
	}

	void WindowHandle::SetWindowTitle(const char* format, ...)
	{
		IWindow* pWindow = HANDLE_UTILS::ResolveHandle(*this);
		if (pWindow != nullptr)
		{
			va_list va;
			va_start(va, format);
			pWindow->SetWindowTitle(format, va);
			va_end(va);
			return;
		}

		LogError("Failed to set window title. Could not resolve window handle!");
	}
}