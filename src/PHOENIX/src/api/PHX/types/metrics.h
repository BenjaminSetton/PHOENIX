#pragma once

#include "BSL/integral_types.h"

namespace PHX
{
	struct Metrics
	{
		// Draw stats
		u32 drawCalls       = 0;
		u32 vertices        = 0;
		u32 indices         = 0;
		u32 triangles       = 0;

		// Uniform updates
		u32 uniformUpdates  = 0;

		// Passes
		u32 passCount       = 0;

		// Resource handles
		u32 bufferCount                = 0;
		u32 textureCount               = 0;
		u32 shaderCount                = 0;
		u32 pipelineCount              = 0;
		u32 uniformCollectionCount     = 0;
		u32 accelerationStructureCount = 0;

		// Total allocated GPU memory in bytes
		u64 allocatedMemoryBytes = 0;

		// GPU frame time in milliseconds
		float gpuFrameTime = 0.0f;
	};
}
