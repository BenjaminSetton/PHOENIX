#pragma once

#include "../types/basic_types.h"

namespace PHX
{
	struct SwapChainCreateInfo
	{
		// TODO
	};

	class ISwapChain
	{
	public:

		virtual ~ISwapChain() { }

		virtual bool Create(const SwapChainCreateInfo& createInfo) = 0;

	};
}