#pragma once

#include "../types/integral_types.h"
#include "../types/shader_desc.h"

namespace PHX
{
	struct ShaderCreateInfo
	{
		void* pBytecode;
		u32 byteCount;
		SHADER_TYPE type;
	};

	class IShader
	{
	public:

		virtual ~IShader() { }

		virtual SHADER_TYPE GetType() const = 0;
	};
}