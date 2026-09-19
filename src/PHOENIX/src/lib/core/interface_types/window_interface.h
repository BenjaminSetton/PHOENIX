#pragma once

#include <stdarg.h> // va_list

#include "BSL/integral_types.h"
#include "core/ref.h"

namespace PHX
{
	class IWindow : public RefCounted
	{
	public:

		virtual ~IWindow() {};

		virtual u32 GetCurrentWidth() const = 0;
		virtual u32 GetCurrentHeight() const = 0;
		virtual float GetContentScale() const = 0;

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