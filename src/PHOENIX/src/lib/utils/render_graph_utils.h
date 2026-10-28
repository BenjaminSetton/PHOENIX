#pragma once

#include <string>
#include <vector>

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
		ResourceID resourceID			= INVALID_RESOURCE_ID;

		u32 passIndex					= U32_MAX;
		RESOURCE_ACCESS access			= RESOURCE_ACCESS::READ_ONLY;

		// Texture only
		ATTACHMENT_TYPE attachmentType	= ATTACHMENT_TYPE::COLOR;
		ATTACHMENT_STORE_OP storeOp		= ATTACHMENT_STORE_OP::IGNORE;
		ATTACHMENT_LOAD_OP loadOp		= ATTACHMENT_LOAD_OP::IGNORE;
		ClearValues clearValue			= {}; // Used when loadOp is set to CLEAR

		// Buffer only
		BufferUsageFlags bufferUsage	= BUFFER_USAGE_FLAG_UNIFORM_BUFFER;
	};

	struct VizUsage
	{
		u32 passIndex					= U32_MAX;
		RESOURCE_ACCESS access			= RESOURCE_ACCESS::READ_ONLY;
		ATTACHMENT_TYPE attachmentType	= ATTACHMENT_TYPE::INVALID;
		ATTACHMENT_LOAD_OP loadOp		= ATTACHMENT_LOAD_OP::INVALID;
		ATTACHMENT_STORE_OP storeOp		= ATTACHMENT_STORE_OP::INVALID;
		BufferUsageFlags bufferUsage	= 0;
		std::string layout;							// Layout the resource is in during the pass. Empty if not applicable
		std::vector<std::string> barrierLines;		// Human-readable description of the barrier before this usage. Empty if none
	};

	struct VizResource
	{
		std::string name; // Empty if unnamed
		RESOURCE_TYPE type = RESOURCE_TYPE::TEXTURE;
		bool isPresent = false;
		std::vector<VizUsage> usages; // Must be sorted by pass index
	};

	struct VizPass
	{
		const char* name = nullptr;	// Null for unused pass slots
		PASS_TYPE passType = PASS_TYPE::GRAPHICS;
		bool isVirtual = false;
	};

	struct VizGraph
	{
		std::vector<VizPass> passes;				// Indexed by pass index
		std::vector<u32> submissionOrder;			// Executed pass indices, in submission order
		std::vector<VizResource> resources;
		u32 finalPassIndex = U32_MAX;
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool IsWriteAccess(RESOURCE_ACCESS access);
	bool IsReadAccess(RESOURCE_ACCESS access);

	ResourceID HashResource(Handle resource, const RESOURCE_TYPE& type);

	QUEUE_TYPE ConvertPassTypeToQueueType(PASS_TYPE passType);
	ATTACHMENT_TYPE CalculateAttachmentType(TextureHandle handle);
	const char* PassTypeToString(PASS_TYPE passType);

	// Creates a render graph visualization using the provided data from the render graph backend (e.g. render_graph_vk)
	STATUS_CODE CreateGraphVisualization(const VizGraph& graph, const char* fileName);
}