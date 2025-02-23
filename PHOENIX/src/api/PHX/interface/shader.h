#pragma once

#include "../types/integral_types.h"
#include "../types/shader_desc.h"

namespace PHX
{
	struct ShaderCreateInfo
	{
		const u32* pBytecode;
		u32 size;
		SHADER_STAGE stage;
	};

	class IShader
	{
	public:

		virtual ~IShader() { }

		virtual SHADER_STAGE GetStage() const = 0;
	};
}