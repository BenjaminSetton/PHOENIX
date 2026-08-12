#pragma once

#if defined(PROFILER_TRACY)

#include <Tracy.hpp>

#define TRACY_ENABLE

#define PROFILE_SCOPE(x) ZoneScoped(x)
#define PROFILE_LOOP(x) FrameMark(x)

#else

#define PROFILE_SCOPE(x)
#define PROFILE_LOOP(x)

#endif // defined(PROFILER_TRACY)