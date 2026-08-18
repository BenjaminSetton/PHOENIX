#pragma once

#if defined(PROFILER_TRACY)

// Ignore "unreachable code" warnings
#pragma warning(disable: 4702)
#include <tracy/Tracy.hpp>

#define PROFILE_SCOPE(name) ZoneScopedN(name)
#define PROFILE_LOOP(name) FrameMarkNamed(name)

#else

#define PROFILE_SCOPE(name)
#define PROFILE_LOOP(name)

#endif