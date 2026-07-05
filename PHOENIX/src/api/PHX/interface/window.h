#pragma once

#include "PHX/interface/handle.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/vec_types.h"

#include <stdarg.h> // TODO - MOVE TO LIB
#include "PHX/interface/ref.h" // TODO - MOVE TO LIB

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

	struct WindowHandle : public Handle
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

	// TODO - MOVE TO LIB
	class IWindow : public RefCounted
	{
	public:

		virtual ~IWindow() {};

		// Returns the current size of the window. This represent the logical size of the OS window, not
		// the renderable space. All rendering-related sizes should use the swap chain's GetWidth() / GetHeight() functions,
		// while these functions can be used for user input coordinates, etc.
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

		virtual void SetWindowTitle(const char* format, va_list args) = 0;
	};

}