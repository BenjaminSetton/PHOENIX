#pragma once

#include "../types/status_code.h"

namespace PHX
{
	// Forward declarations
	class IWindow;

	typedef void(*DebugMessageCallbackFn)(const char* msg);

	struct RenderDeviceCreateInfo
	{
		DebugMessageCallbackFn debugMessageCallback = nullptr;
		IWindow* window								= nullptr;
	};

	class IRenderDevice
	{
	public:

		virtual ~IRenderDevice() { }

		// Query device stats
		virtual const char* GetDeviceName() const = 0;

		// Allocations
		virtual STATUS_CODE AllocateBuffer()        = 0;
		virtual STATUS_CODE AllocateFramebuffer()   = 0;
		virtual STATUS_CODE AllocateCommandBuffer() = 0;
		virtual STATUS_CODE AllocateTexture()       = 0;
		virtual STATUS_CODE AllocateShader()        = 0;
	};
}
