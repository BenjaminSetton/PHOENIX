#pragma once

namespace PHX
{
	class IRenderDevice
	{
	public:

		// Query device stats

		// Allocations
		virtual bool AllocateBuffer()        = 0;
		virtual bool AllocateCommandBuffer() = 0;
		virtual bool AllocateTexture()       = 0;

	private:
	};
}
