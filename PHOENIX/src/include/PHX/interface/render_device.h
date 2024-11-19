#pragma once

#include "../types/basic_types.h"

namespace PHX
{
	// Forward declarations
	class IWindow;

	typedef void(*DebugMessageCallbackFn)(const char* msg);

	struct RenderDeviceCreateInfo
	{
		const char* applicationName					= nullptr;
		const char* engineName						= nullptr;
		const char** validationLayerNames			= nullptr;
		u32 validationLayerCount					= 0;
		DebugMessageCallbackFn debugMessageCallback = nullptr;
		IWindow* window								= nullptr;
	};

	class IRenderDevice
	{
	public:

		virtual ~IRenderDevice() { }

		// Query device stats

		// Allocations
		virtual bool AllocateBuffer()        = 0;
		virtual bool AllocateCommandBuffer() = 0;
		virtual bool AllocateTexture()       = 0;
		virtual bool AllocateShader()        = 0;
	};
}
