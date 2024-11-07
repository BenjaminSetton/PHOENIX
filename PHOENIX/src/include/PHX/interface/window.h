#pragma once

#include "../types/basic_types.h"

namespace PHX
{
	typedef void(*fp_onWindowResized)(u32 newWidth, u32 newHeight);
	typedef void(*fp_onWindowFocusChanged)(bool isFocused);

	enum class CURSOR_TYPE
	{
		SHOWN = 0, // OS cursor is shown
		HIDDEN,    // OS cursor is hidden and unbound (can be moved off-screen)
		DISABLED   // OS cursor is hidden and bound (unlimited mouse movement within screen)
	};

	struct WindowCreateInfo
	{
		const char* title                                  = nullptr;
		u32 width                                          = 1920;
		u32 height                                         = 1080;
		CURSOR_TYPE cursorType                             = CURSOR_TYPE::SHOWN;
		fp_onWindowResized windowResizedCallback           = nullptr;
		fp_onWindowFocusChanged windowFocusChangedCallback = nullptr;
	};

	class IWindow
	{
	public:

		virtual ~IWindow() {};

		virtual u32 GetCurrentWidth() = 0;
		virtual u32 GetCurrentHeight() = 0;
		virtual const char* GetName() = 0;

		virtual void Update(float deltaTime) = 0;
		virtual bool InFocus() = 0;
		virtual bool ShouldClose() = 0;

		virtual void SetWindowTitle(const char* format, ...) = 0;
	};

}