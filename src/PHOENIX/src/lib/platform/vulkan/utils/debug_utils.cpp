#include "debug_utils.h"

#include "BSL/logger.h"
#include "BSL/sanity.h"
#include "core/global_settings.h"
#include <vulkan/vk_enum_string_helper.h>

using namespace BSL;

namespace PHX
{
	namespace DEBUG_UTILS
	{
#if defined(PHX_DEBUG)
		void SetObjectName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* name)
		{
			const Settings& settings = GlobalSettings::Get().GetSettings();
			if (!settings.enableValidation)
			{
				LogWarning("Could not set debug name on Vulkan object. Validation layers are disabled!");
				return;
			}

			if (device == VK_NULL_HANDLE || objectHandle == 0 || name == nullptr)
			{
				LogWarning("Failed to set debug name on Vulkan object. One or more parameters are invalid!");
				return;
			}

			PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
			if (pfnSetDebugUtilsObjectNameEXT == nullptr)
			{
				LogWarning("Failed to set debug name on Vulkan object. vkSetDebugUtilsObjectNameEXT function pointer is null!");
				return;
			}

			VkDebugUtilsObjectNameInfoEXT nameInfo{};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = objectType;
			nameInfo.objectHandle = objectHandle;
			nameInfo.pObjectName = name;

			VkResult res = pfnSetDebugUtilsObjectNameEXT(device, &nameInfo);
			if (res != VK_SUCCESS)
			{
				LogWarning("Failed to set debug name '%s' on Vulkan object (type=%u, handle=0x%llx). Got error: \"%s\"",
					name, static_cast<u32>(objectType), static_cast<unsigned long long>(objectHandle), string_VkResult(res));
			}
		}

		void BeginLabel(VkDevice device, VkCommandBuffer commandBuffer, const char* name)
		{
			if (device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE || name == nullptr)
			{
				return;
			}

			PFN_vkCmdBeginDebugUtilsLabelEXT pfnBegin = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
			if (pfnBegin == nullptr)
			{
				return;
			}

			VkDebugUtilsLabelEXT labelInfo{};
			labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			labelInfo.pLabelName = name;
			pfnBegin(commandBuffer, &labelInfo);
		}

		void EndLabel(VkDevice device, VkCommandBuffer commandBuffer)
		{
			if (device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE)
			{
				return;
			}

			PFN_vkCmdEndDebugUtilsLabelEXT pfnEnd = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
			if (pfnEnd == nullptr)
			{
				return;
			}

			pfnEnd(commandBuffer);
		}
#else
		void SetObjectName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* name)
		{
			/* No-op */
			UNUSED(device);
			UNUSED(objectType);
			UNUSED(objectHandle);
			UNUSED(name);
		}

		void BeginLabel(VkDevice device, VkCommandBuffer commandBuffer, const char* name)
		{
			/* No-op */
			UNUSED(device);
			UNUSED(commandBuffer);
			UNUSED(name);
		}

		void EndLabel(VkDevice device, VkCommandBuffer commandBuffer)
		{
			/* No-op */
			UNUSED(device);
			UNUSED(commandBuffer);
		}
#endif
	}
}
