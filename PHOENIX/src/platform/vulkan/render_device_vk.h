#pragma once

#include "../../interface/render_device.h"

namespace PHX
{
	class RenderDeviceVk : public IRenderDevice
	{
		RenderDeviceVk();
		~RenderDeviceVk();

		bool AllocateBuffer() override;
		bool AllocateCommandBuffer() override;
		bool AllocateTexture() override;
	};
}