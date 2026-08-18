#pragma once

#if defined(PROFILER_TRACY)

// Ignore "unreachable code" warnings
//#pragma warning(disable: 4702)
#include <tracy/TracyVulkan.hpp>

#define PROFILE_VKCONTEXT_CREATE(physDevice, logDevice, queue, cmdBuffer) TracyVkContext(physDevice, logDevice, queue, cmdBuffer)
#define PROFILE_VKCONTEXT_DESTROY(context) TracyVkDestroy(context)
#define PROFILE_VKCONTEXT_NAME(context, name, size) TracyVkContextName(context, name, size)
#define PROFILE_VK_ZONE(context, cmdBuffer, name) TracyVkZone(context, cmdBuffer, name)
#define PROFILE_VK_COLLECT(context, cmdBuffer) TracyVkCollect(context, cmdBuffer)

#else

#define PROFILE_VKCONTEXT_CREATE(physDevice, logDevice, queue, cmdBuffer)
#define PROFILE_VKCONTEXT_DESTROY(context)
#define PROFILE_VKCONTEXT_NAME(context, name, size)
#define PROFILE_VK_ZONE(context, cmdBuffer, name)
#define PROFILE_VK_COLLECT(context, cmdBuffer)

#endif