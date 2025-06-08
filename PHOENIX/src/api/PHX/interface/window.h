#pragma once

#include "../types/integral_types.h"
#include "../types/vec_types.h"

namespace PHX
{
	enum class CURSOR_TYPE
	{
		SHOWN = 0, // OS cursor is shown
		HIDDEN,    // OS cursor is hidden and unbound (can be moved off-screen)
		DISABLED   // OS cursor is hidden and bound (unlimited mouse movement within screen)
	};

	struct WindowCreateInfo
	{
		const char* title		= nullptr;
		Vec2u size				= { 1920, 1080 };
		Vec2u position			= { 0 , 0 };
		CURSOR_TYPE cursorType	= CURSOR_TYPE::SHOWN;
		bool canResize			= true;
	};

	class IWindow
	{
	public:

		virtual ~IWindow() {};

		virtual u32 GetCurrentWidth() const = 0;
		virtual u32 GetCurrentHeight() const = 0;
		virtual int GetPositionX() const = 0;
		virtual int GetPositionY() const = 0;
		virtual const char* GetName() const = 0;

		virtual void Update(float deltaTime) = 0;
		virtual bool InFocus() const = 0;
		virtual bool ShouldClose() const = 0;
		virtual bool IsMinimized() const = 0;
		virtual bool IsMaximized() const = 0;


		virtual void SetWindowTitle(const char* format, ...) = 0;
	};

}