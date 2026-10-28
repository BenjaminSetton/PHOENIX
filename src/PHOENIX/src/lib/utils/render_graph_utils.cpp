
#include <BSL/logger.h>

#include "utils/cache_utils.h"
#include "utils/render_graph_utils.h"

using namespace BSL;

namespace PHX
{
	bool IsWriteAccess(RESOURCE_ACCESS access)
	{
		return ((access == RESOURCE_ACCESS::READ_WRITE) || (access == RESOURCE_ACCESS::WRITE_ONLY));
	}

	bool IsReadAccess(RESOURCE_ACCESS access)
	{
		return ((access == RESOURCE_ACCESS::READ_WRITE) || (access == RESOURCE_ACCESS::READ_ONLY));
	}

	ResourceID HashResource(Handle resource, const RESOURCE_TYPE& type)
	{
		size_t seed = 0;
		HashCombine(seed, resource.GetIndex());
		HashCombine(seed, type);

		return static_cast<ResourceID>(seed);
	}

	QUEUE_TYPE ConvertPassTypeToQueueType(PASS_TYPE passType)
	{
		switch (passType)
		{
		case PASS_TYPE::COMPUTE:      return QUEUE_TYPE::COMPUTE;
		case PASS_TYPE::GRAPHICS:     return QUEUE_TYPE::GRAPHICS;
		case PASS_TYPE::TRANSFER:     return QUEUE_TYPE::TRANSFER;
		case PASS_TYPE::RAY_TRACING:  return QUEUE_TYPE::GRAPHICS;
		case PASS_TYPE::AS_BUILD:     return QUEUE_TYPE::GRAPHICS;
		default:
		{
			break;
		}
		}

		ASSERT_ALWAYS("Failed to convert pass type to queue type!");
		return QUEUE_TYPE::GRAPHICS;
	}

	ATTACHMENT_TYPE CalculateAttachmentType(TextureHandle handle)
	{
		AspectTypeFlags aspectFlags = handle.GetAspectFlags();
		switch (aspectFlags)
		{
		case ASPECT_TYPE_FLAG_COLOR:                              return ATTACHMENT_TYPE::COLOR;
		case (ASPECT_TYPE_FLAG_DEPTH | ASPECT_TYPE_FLAG_STENCIL): return ATTACHMENT_TYPE::DEPTH_STENCIL;
		case ASPECT_TYPE_FLAG_DEPTH:                              return ATTACHMENT_TYPE::DEPTH;
		case ASPECT_TYPE_FLAG_STENCIL:                            return ATTACHMENT_TYPE::STENCIL;
		default:
		{
			break;
		}
		}

		LogError("Failed to calculate attachment type from texture's aspect flags. No valid combination was found for aspect flags %u", aspectFlags);
		return ATTACHMENT_TYPE::INVALID;
	}
}