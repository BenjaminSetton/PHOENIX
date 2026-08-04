#pragma once

#include <limits>
#include <random>

namespace Common
{
	typedef uint64_t UUID;

	static constexpr UUID INVALID_UUID = std::numeric_limits<UUID>::max();

	UUID GetUUID();
}