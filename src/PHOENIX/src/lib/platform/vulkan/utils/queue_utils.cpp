
#include <vulkan/vk_enum_string_helper.h>

#include "BSL/logger.h"
#include "BSL/sanity.h"
#include "queue_utils.h"

using namespace BSL;

namespace PHX
{
	// If anything changes with the QUEUE_TYPE enum, make sure to change every reference to queueFamilies below!
	STATIC_ASSERT(static_cast<u32>(QUEUE_TYPE::COUNT) == 4);

	QueueFamilyIndices::QueueFamilyIndices()
	{
		queueFamilies[QUEUE_TYPE::GRAPHICS] = { INVALID_INDEX, INVALID_INDEX };
		queueFamilies[QUEUE_TYPE::PRESENT ] = { INVALID_INDEX, INVALID_INDEX };
		queueFamilies[QUEUE_TYPE::TRANSFER] = { INVALID_INDEX, INVALID_INDEX };
		queueFamilies[QUEUE_TYPE::COMPUTE ] = { INVALID_INDEX, INVALID_INDEX };
		queueFamilies[QUEUE_TYPE::COUNT   ] = { INVALID_INDEX, INVALID_INDEX };
	}

	QueueFamilyIndices::~QueueFamilyIndices()
	{
		// Nothing to do here
	}

	QueueFamilyIndices::QueueFamilyIndices(const QueueFamilyIndices& other) : queueFamilies(other.queueFamilies)
	{
	}

	QueueFamilyIndices::QueueFamilyIndices(QueueFamilyIndices&& other) noexcept : queueFamilies(std::move(other.queueFamilies))
	{
	}

	QueueFamilyIndices& QueueFamilyIndices::operator=(const QueueFamilyIndices& other)
	{
		if (this == &other) return *this;

		queueFamilies[QUEUE_TYPE::GRAPHICS] = other.queueFamilies.at(QUEUE_TYPE::GRAPHICS);
		queueFamilies[QUEUE_TYPE::PRESENT ] = other.queueFamilies.at(QUEUE_TYPE::PRESENT );
		queueFamilies[QUEUE_TYPE::TRANSFER] = other.queueFamilies.at(QUEUE_TYPE::TRANSFER);
		queueFamilies[QUEUE_TYPE::COMPUTE ] = other.queueFamilies.at(QUEUE_TYPE::COMPUTE );
		queueFamilies[QUEUE_TYPE::COUNT   ] = other.queueFamilies.at(QUEUE_TYPE::COUNT   );

		return *this;
	}

	void QueueFamilyIndices::SetIndices(QUEUE_TYPE type, u32 familyIndex, u32 queueIndex)
	{
		if (type == QUEUE_TYPE::COUNT) return;

		queueFamilies[type] = { familyIndex, queueIndex };
	}

	u32 QueueFamilyIndices::GetQueueIndex(QUEUE_TYPE type) const
	{
		if (type == QUEUE_TYPE::COUNT) return INVALID_INDEX;

		return queueFamilies.at(type).queueIndex;
	}

	u32 QueueFamilyIndices::GetFamilyIndex(QUEUE_TYPE type) const
	{
		if (type == QUEUE_TYPE::COUNT) return INVALID_INDEX;

		return queueFamilies.at(type).queueFamilyIndex;
	}

	bool QueueFamilyIndices::IsValid(IndexPair indexPair) const
	{
		return (indexPair.queueFamilyIndex != INVALID_INDEX && indexPair.queueIndex != INVALID_INDEX);
	}

	bool QueueFamilyIndices::IsComplete()
	{
		return IsValid(queueFamilies[QUEUE_TYPE::GRAPHICS]) &&
		       IsValid(queueFamilies[QUEUE_TYPE::PRESENT ]) &&
		       IsValid(queueFamilies[QUEUE_TYPE::COMPUTE ]) &&
		       IsValid(queueFamilies[QUEUE_TYPE::TRANSFER]);
	}

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		// PLAN:
		// - Loop through all queue families.
		// - Try to find queues for all supported QUEUE_TYPE values
		// - Try to find separate TRANSFER queue (even if within same queue family)
		// - Try to find separate PRESENT queue (even if within same queue family)
		// - Optimal scenario could be that all QUEUE_TYPE values have distinct
		//   queue indices, but it's fine if GRAPHICS and COMPUTE share a queue for now.
		// - PRESENT MUST have it's own queue to avoid blocking subsequent frames in flight
		//   on work that's currently in-flight. This is seen through a very long fence wait in DeviceContextVk::BeginFrame()

		QueueFamilyIndices indices;

		// TEMP
		indices.SetIndices(QUEUE_TYPE::COMPUTE, 0, 0);
		indices.SetIndices(QUEUE_TYPE::GRAPHICS, 0, 0);
		indices.SetIndices(QUEUE_TYPE::PRESENT, 0, 0);
		indices.SetIndices(QUEUE_TYPE::TRANSFER, 0, 1);
		return indices;
		// TEMP

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		LogDebug("Found %u queue families:", queueFamilies.size());
		uint32_t graphicsTransferQueue = std::numeric_limits<uint32_t>::max();
		uint32_t i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			// Check that the device supports a graphics queue and compute queue
			// NOTE - We could potentially select separate queues for graphics and
			//        compute, but let's keep it simple for now
			if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
			{
				indices.SetIndices(QUEUE_TYPE::GRAPHICS, i, 0);
				indices.SetIndices(QUEUE_TYPE::COMPUTE, i, 0);
			}

			// Check that the device supports present queues
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			LogDebug("\t[%u] - %u queues: %s, PresentSupported(%s) ", 
				i, 
				queueFamily.queueCount, 
				string_VkQueueFlags(queueFamily.queueFlags).c_str(),
				presentSupport ? "YES" : "NO");

			if (presentSupport)
			{
				indices.SetIndices(QUEUE_TYPE::PRESENT, i, 0);
			}

			// Check that the device supports a transfer queue
			if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				// Choose a different queue from the graphics queue, if possible
				if (indices.GetQueueIndex(QUEUE_TYPE::GRAPHICS) == i)
				{
					graphicsTransferQueue = i;
				}
				else
				{
					indices.SetIndices(QUEUE_TYPE::TRANSFER, i, 0);
				}
			}

			i++;
		}

		// If we couldn't find a different queue for the TRANSFER and GRAPHICS operations, then simply
		// use the same queue for both (if it supports TRANSFER operations)
		if (!indices.IsValid({ 0, static_cast<u32>(QUEUE_TYPE::TRANSFER) }) && graphicsTransferQueue != QueueFamilyIndices::INVALID_INDEX)
		{
			LogDebug("Failed to find separate queue family for transfer and graphics. Using the same queue for both operations!");
			indices.SetIndices(QUEUE_TYPE::TRANSFER, 0, graphicsTransferQueue);
		}

		// Check that we filled in all of our queue families, otherwise log a warning
		if (!indices.IsComplete())
		{
			LogError("Failed to find all queue families!");
		}

		return indices;
	}

	bool NeedsSynchronization(QUEUE_TYPE type)
	{
		return (type != QUEUE_TYPE::PRESENT);
	}

	const char* GetQueueTypeName(QUEUE_TYPE type)
	{
		switch (type)
		{
		case QUEUE_TYPE::GRAPHICS: return "GRAPHICS";
		case QUEUE_TYPE::PRESENT:  return "PRESENT";
		case QUEUE_TYPE::TRANSFER: return "TRANSFER";
		case QUEUE_TYPE::COMPUTE:  return "COMPUTE";
		}

		ASSERT_ALWAYS("Failed to get name for queue type. Unexpected value!");
		return "Unknown";
	}
}

