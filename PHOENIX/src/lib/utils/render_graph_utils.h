#pragma once

#include "PHX/types/attachment_desc.h"
#include "PHX/types/clear_color.h"

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
		Handle handle		= INVALID_HANDLE;
		RESOURCE_TYPE type	= RESOURCE_TYPE::TEXTURE;

		u64 resourceID		= U64_MAX;
	};

	struct ResourceUsage
	{
		RESOURCE_IO io					= RESOURCE_IO::INPUT;

		// Texture only
		ATTACHMENT_TYPE attachmentType	= ATTACHMENT_TYPE::COLOR;
		ATTACHMENT_STORE_OP storeOp		= ATTACHMENT_STORE_OP::IGNORE;
		ATTACHMENT_LOAD_OP loadOp		= ATTACHMENT_LOAD_OP::IGNORE;
		ClearValues clearValue			= {};	// Used when loadOp == CLEAR

		// Buffer only
		BUFFER_USAGE bufferUsage		= BUFFER_USAGE::UNIFORM_BUFFER;

		u64 resourceID					= U64_MAX;	// ID of the physical resource this usage is linked to
		u32 passIndex					= U32_MAX;	// Index of the render pass that uses this resource
	};
}