#pragma once

#include <stdarg.h> // va_list

#include "core/ref.h"
#include "PHX/types/integral_types.h"

namespace PHX
{
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