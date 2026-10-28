#pragma once

#include "PHX/interface/handle.h"
#include "PHX/interface/render_graph.h"
#include "PHX/types/attachment_desc.h"
#include "PHX/types/buffer_desc.h"
#include "PHX/types/clear_color.h"
#include "PHX/types/queue_type.h"

namespace PHX
{
	using ResourceID = u64;
	static constexpr ResourceID INVALID_RESOURCE_ID = U64_MAX;

	enum class RESOURCE_TYPE
	{
		TEXTURE,				// ITexture
		BUFFER,					// IBuffer
		UNIFORM,				// IUniformCollection
		ACCELERATION_STRUCTURE	// IAccelerationStructure
	};

	enum class RESOURCE_ACCESS
	{
		READ_ONLY,
		WRITE_ONLY,
		READ_WRITE
	};

	struct RenderResource
	{
		ResourceID resourceID	= INVALID_RESOURCE_ID;

		Handle handle			= INVALID_HANDLE;
		RESOURCE_TYPE type		= RESOURCE_TYPE::TEXTURE;
	};

	struct ResourceUsage
	{
		ResourceID resourceID			= INVALID_RESOURCE_ID;	// ID of the physical resource this usage is linked to

		u32 passIndex					= U32_MAX;	// Index of the render pass that uses this resource
		RESOURCE_ACCESS access			= RESOURCE_ACCESS::READ_ONLY;

		// Texture only
		ATTACHMENT_TYPE attachmentType	= ATTACHMENT_TYPE::COLOR;
		ATTACHMENT_STORE_OP storeOp		= ATTACHMENT_STORE_OP::IGNORE;
		ATTACHMENT_LOAD_OP loadOp		= ATTACHMENT_LOAD_OP::IGNORE;
		ClearValues clearValue			= {};	// Used when loadOp == CLEAR

		// Buffer only
		BufferUsageFlags bufferUsage	= BUFFER_USAGE_FLAG_UNIFORM_BUFFER;
	};

	bool IsWriteAccess(RESOURCE_ACCESS access);
	bool IsReadAccess(RESOURCE_ACCESS access);

	ResourceID HashResource(Handle resource, const RESOURCE_TYPE& type);

	QUEUE_TYPE ConvertPassTypeToQueueType(PASS_TYPE passType);
	ATTACHMENT_TYPE CalculateAttachmentType(TextureHandle handle);
}