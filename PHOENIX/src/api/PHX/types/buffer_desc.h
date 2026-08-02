#pragma once

#include "PHX/types/integral_types.h"

namespace PHX
{
	// Buffer usage flags. These are bit flags (not a single-value enum) because a buffer can
	// have multiple usages at once (e.g. a GPU-written indirect-args buffer is both STORAGE and
	// INDIRECT). This mirrors the USAGE_TYPE_FLAG / UsageTypeFlags pattern used for textures.
	// INVALID is 0; all valid flags start at (1 << 0).
	//
	// Note: TRANSFER_SRC and TRANSFER_DST are intentionally NOT exposed here. TRANSFER_DST is
	// force-OR'd onto every buffer in BufferVk::BufferVk because CopyDataToBuffer uses a staging
	// copy (vkCmdCopyBuffer) for all non-uniform buffers, and requiring every call site to
	// remember the flag is a footgun. TRANSFER_SRC is not currently needed by any API; if a
	// readback/copy-from-buffer API is added in the future, it should be handled internally
	// rather than exposing the flag to the client.
	enum BUFFER_USAGE_FLAG : u32
	{
		BUFFER_USAGE_FLAG_INVALID                            = 0,
		BUFFER_USAGE_FLAG_UNIFORM_BUFFER                     = (1 << 0),
		BUFFER_USAGE_FLAG_STORAGE_BUFFER                     = (1 << 1),
		BUFFER_USAGE_FLAG_VERTEX_BUFFER                      = (1 << 2),
		BUFFER_USAGE_FLAG_INDEX_BUFFER                       = (1 << 3),
		BUFFER_USAGE_FLAG_INDIRECT_BUFFER                    = (1 << 4),
		BUFFER_USAGE_FLAG_ACCELERATION_STRUCTURE             = (1 << 5),
		BUFFER_USAGE_FLAG_ACCELERATION_STRUCTURE_BUILD_INPUT = (1 << 6),
	};
	using BufferUsageFlags = u32;

	enum class INDEX_TYPE
	{
		U16 = 0,
		U32,

		MAX
	};
}