
#include "render_device_vk.h"
#include "../../utils/logger.h"

namespace PHX
{
	RenderDeviceVk::RenderDeviceVk()
	{
		LogInfo("Constructed Vk device!");
	}

	RenderDeviceVk::~RenderDeviceVk()
	{
		LogInfo("Destructed Vk device!");
	}

	bool RenderDeviceVk::AllocateBuffer()
	{
		LogInfo("Allocated buffer!");
		return true; 
	}

	bool RenderDeviceVk::AllocateCommandBuffer()
	{
		LogInfo("Allocated command buffer!"); 
		return true; 
	}

	bool RenderDeviceVk::AllocateTexture()
	{
		LogInfo("Allocated texture!");
		return true;
	}
}

