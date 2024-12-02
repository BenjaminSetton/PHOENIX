#pragma once

#include "basic_types.h"

namespace PHX
{
	enum class GRAPHICS_API : u8
	{
		VULKAN = 0,
		OPENGL,
		DX11,
		DX12
	};
}