
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
		QueueFamilyIndices indices;

		u32 queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		// Tracks how many queue indices have already been taken up from each family
		std::vector<u32> nextFreeQueueInFamily(queueFamilyCount, 0);

		auto ClaimQueueIndex = [&](u32 familyIndex) -> u32
		{
			const u32 queueCount = queueFamilies[familyIndex].queueCount;
			const u32 nextFree = nextFreeQueueInFamily[familyIndex];
			if (nextFree < queueCount)
			{
				nextFreeQueueInFamily[familyIndex] = nextFree + 1;
				return nextFree;
			}

			// Family is out of unclaimed queues; fall back to reusing the last valid index
			// rather than handing out something out of range
			LogWarning("Queue family %u has no more unclaimed queues left (total %u). Reusing an existing queue index!", familyIndex, queueCount);
			return queueCount > 0 ? queueCount - 1 : 0;
		};

		LogDebug("Found %u queue families:", queueFamilyCount);

		u32 dedicatedComputeFamily  = QueueFamilyIndices::INVALID_INDEX; // Family with COMPUTE but not GRAPHICS (async-compute)
		u32 dedicatedTransferFamily = QueueFamilyIndices::INVALID_INDEX; // Family with TRANSFER but not GRAPHICS/COMPUTE (async-transfer)

		for (u32 i = 0; i < queueFamilyCount; i++)
		{
			const VkQueueFamilyProperties& queueFamily = queueFamilies[i];

			const bool supportsGraphics = (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
			const bool supportsCompute  = (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT ) != 0;
			const bool supportsTransfer = (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;

			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			LogDebug("\t[%u] - %u queues: %s, PresentSupported(%s) ", 
				i, 
				queueFamily.queueCount, 
				string_VkQueueFlags(queueFamily.queueFlags).c_str(),
				presentSupport ? "YES" : "NO");

			// GRAPHICS
			// Take the first family that supports it. The Vulkan spec guarantees any
			// family that supports GRAPHICS also supports COMPUTE, so this family is always a
			// valid fallback home for COMPUTE too
			if (indices.GetFamilyIndex(QUEUE_TYPE::GRAPHICS) == QueueFamilyIndices::INVALID_INDEX && supportsGraphics)
			{
				u32 queueIndex = ClaimQueueIndex(i);
				indices.SetIndices(QUEUE_TYPE::GRAPHICS, i, queueIndex);
			}

			// PRESENT
			// Required to be in it's own queue. Prefer the graphics family if it supports presenting,
			// since most drivers require present-capable queues to also support graphics. Otherwise
			// take whatever present-capable family we find
			if (presentSupport)
			{
				const bool alreadyAssigned = indices.GetFamilyIndex(QUEUE_TYPE::PRESENT) != QueueFamilyIndices::INVALID_INDEX;
				const bool isGraphicsFamily = indices.GetFamilyIndex(QUEUE_TYPE::GRAPHICS) == i;

				if (!alreadyAssigned || isGraphicsFamily)
				{
					u32 queueIndex = ClaimQueueIndex(i);
					indices.SetIndices(QUEUE_TYPE::PRESENT, i, queueIndex);
				}
			}

			// Store async-compute queue, if applicable
			if (supportsCompute && !supportsGraphics && dedicatedComputeFamily == QueueFamilyIndices::INVALID_INDEX)
			{
				dedicatedComputeFamily = i;
			}

			// Store async-transfer queue, if applicable
			if (supportsTransfer && !supportsGraphics && !supportsCompute && dedicatedTransferFamily == QueueFamilyIndices::INVALID_INDEX)
			{
				dedicatedTransferFamily = i;
			}
		}

		if (dedicatedComputeFamily != QueueFamilyIndices::INVALID_INDEX)
		{
			u32 queueIndex = ClaimQueueIndex(dedicatedComputeFamily);
			indices.SetIndices(QUEUE_TYPE::COMPUTE, dedicatedComputeFamily, queueIndex);
		}
		else if (indices.GetFamilyIndex(QUEUE_TYPE::GRAPHICS) != QueueFamilyIndices::INVALID_INDEX)
		{
			LogInfo("Could not find a dedicated queue family for async compute. Sharing the graphics queue instead!");
			u32 graphicsFamily = indices.GetFamilyIndex(QUEUE_TYPE::GRAPHICS);
			indices.SetIndices(QUEUE_TYPE::COMPUTE, graphicsFamily, indices.GetQueueIndex(QUEUE_TYPE::GRAPHICS));
		}

		// Prefer a truly dedicated transfer family/queue if one was found
		if (dedicatedTransferFamily != QueueFamilyIndices::INVALID_INDEX)
		{
			u32 queueIndex = ClaimQueueIndex(dedicatedTransferFamily);
			indices.SetIndices(QUEUE_TYPE::TRANSFER, dedicatedTransferFamily, queueIndex);
		}
		else if (indices.GetFamilyIndex(QUEUE_TYPE::GRAPHICS) != QueueFamilyIndices::INVALID_INDEX)
		{
			// No dedicated transfer family exists. Try to claim a distinct queue *index* within
			// the graphics family so transfer work isn't forced to share a queue object with
			// render work; if the family is out of spare queues this just reuses one instead
			LogDebug("Could not find a dedicated queue family for transfer. Using a queue from the graphics family instead!");
			u32 graphicsFamily = indices.GetFamilyIndex(QUEUE_TYPE::GRAPHICS);
			u32 queueIndex = ClaimQueueIndex(graphicsFamily);
			indices.SetIndices(QUEUE_TYPE::TRANSFER, graphicsFamily, queueIndex);
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

	bool CanProfileQueueType(QUEUE_TYPE type)
	{
		switch (type)
		{
		case QUEUE_TYPE::GRAPHICS:
		case QUEUE_TYPE::COMPUTE:
		{
			return true;
		}
		default:
		{
			break;
		}
		}

		return false;
	}
}

