#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/shader_desc.h"
#include "PHX/interface/handle.h"
#include "PHX/interface/ref.h" // TODO - Move to lib

namespace PHX
{
	struct ShaderCreateInfo
	{
		const u32* pBytecode;
		u32 size;
		SHADER_STAGE stage;
		ShaderReflectionData reflectionData;
	};

	struct ShaderHandle : public Handle
	{
		DECLARE_HANDLE(ShaderHandle)

		SHADER_STAGE GetStage() const;
		const ShaderReflectionData& GetReflectionData() const;
	};


	// TODO - Move to lib
	class IShader : public RefCounted
	{
	public:

		virtual ~IShader() { }

		virtual SHADER_STAGE GetStage() const = 0;
		virtual const ShaderReflectionData& GetReflectionData() const = 0;
	};
}