#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/shader_desc.h"
#include "PHX/interface/handle.h"

namespace PHX
{
	struct ShaderCreateInfo
	{
		const u32* pBytecode;
		u32 size;
		SHADER_STAGE stage;
		ShaderReflectionData reflectionData;
	};

	struct PHX_API ShaderHandle : public Handle
	{
		DECLARE_PHX_HANDLE(ShaderHandle);

		SHADER_STAGE GetStage() const;
		const ShaderReflectionData& GetReflectionData() const;
	};
}