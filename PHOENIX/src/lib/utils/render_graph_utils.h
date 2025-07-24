#pragma once

#include "PHX/types/attachment_desc.h"

namespace PHX
{
	enum class RESOURCE_TYPE
	{
		TEXTURE,	// ITexture
		BUFFER,		// IBuffer
		UNIFORM		// IUniformCollection
	};

	enum class RESOURCE_IO
	{
		INPUT,
		OUTPUT
	};

	struct RenderResource
	{
		void* data			= nullptr;
		RESOURCE_TYPE type	= RESOURCE_TYPE::TEXTURE;

		u64 resourceID		= U64_MAX;
	};

	struct ResourceUsage
	{
		const char* name				= nullptr;
		RESOURCE_IO io					= RESOURCE_IO::INPUT;
		ATTACHMENT_TYPE attachmentType	= ATTACHMENT_TYPE::COLOR;
		ATTACHMENT_STORE_OP storeOp		= ATTACHMENT_STORE_OP::IGNORE;
		ATTACHMENT_LOAD_OP loadOp		= ATTACHMENT_LOAD_OP::IGNORE;

		u64 resourceID					= U64_MAX;	// ID of the physical resource this usage is linked to
		u32 passIndex					= U32_MAX;	// Index of the render pass that uses this resource
	};
}