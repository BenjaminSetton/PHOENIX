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

	struct ResourceDesc
	{
		const char* name;
		RESOURCE_TYPE resourceType;
		void* data;
		RESOURCE_IO io;
		ATTACHMENT_TYPE attachmentType;
		ATTACHMENT_STORE_OP storeOp;
		ATTACHMENT_LOAD_OP loadOp;
	};
}