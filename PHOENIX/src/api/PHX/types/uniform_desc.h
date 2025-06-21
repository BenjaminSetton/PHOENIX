#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/shader_desc.h"

namespace PHX
{
	enum class UNIFORM_TYPE
	{
		SAMPLER = 0,
		COMBINED_IMAGE_SAMPLER,
		SAMPLED_IMAGE,
		STORAGE_IMAGE,
		UNIFORM_BUFFER,
		STORAGE_BUFFER,
		INPUT_ATTACHMENT,

		MAX
	};

	// AKA a Vulkan descriptor
	struct UniformData
	{
		u32 binding;
		UNIFORM_TYPE type;
		SHADER_STAGE shaderStage;

		////////
		bool operator==(const UniformData& other) const;
		////////
	};

	// AKA a Vulkan descriptor set
	struct UniformDataGroup
	{
		u32 set;
		UniformData* uniformArray;
		u32 uniformArrayCount;

		////////
		bool operator==(const UniformDataGroup& other) const;
		////////
	};
}