#pragma once

#if defined(PROFILER_TRACY)

// [4702] Ignore "unreachable code" warnings
#pragma warning(push)
#pragma warning(disable: 4702)
#include <tracy/Tracy.hpp>
#pragma warning(pop)

#define PROFILE_SCOPE(name) ZoneScopedN(name)
#define PROFILE_LOOP(name) FrameMarkNamed(name)

#else

#define PROFILE_SCOPE(name)
#define PROFILE_LOOP(name)

#endif