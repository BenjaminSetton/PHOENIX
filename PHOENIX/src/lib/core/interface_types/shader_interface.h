#pragma once

#include "PHX/interface/ref.h"
#include "PHX/types/shader_desc.h"

namespace PHX
{
	class IShader : public RefCounted
	{
	public:

		virtual ~IShader() { }

		virtual SHADER_STAGE GetStage() const = 0;
		virtual const ShaderReflectionData& GetReflectionData() const = 0;
	};
}