#pragma once

#include "BSL/integral_types.h"
#include "BSL/vec_types.h"
#include "PHX/interface/handle.h"

namespace PHX
{
	enum class CURSOR_TYPE
	{
		SHOWN = 0, // OS cursor is shown
		HIDDEN,    // OS cursor is hidden and unbound (can be moved off-screen)
		DISABLED   // OS cursor is hidden and bound (unlimited mouse movement within screen)
	};

	enum class WINDOW_MODE
	{
		WINDOWED = 0,
		FULLSCREEN
	};

	struct WindowCreateInfo
	{
		const char* title		= nullptr;
		BSL::Vec2u size			= { 1920, 1080 };
		BSL::Vec2u position		= { 0, 0 };
		CURSOR_TYPE cursorType	= CURSOR_TYPE::SHOWN;
		WINDOW_MODE windowMode	= WINDOW_MODE::WINDOWED;
		bool canResize			= true;
	};

	struct PHX_API WindowHandle : public Handle
	{
		DECLARE_PHX_HANDLE(WindowHandle);

		u32 GetCurrentWidth() const;
		u32 GetCurrentHeight() const;

		int GetPositionX() const;
		int GetPositionY() const;
		const char* GetName() const;

		void Update(float deltaTime);
		bool InFocus() const;
		bool ShouldClose() const;
		bool IsMinimized() const;
		bool IsMaximized() const;


		void SetWindowTitle(const char* format, ...);
	};
}