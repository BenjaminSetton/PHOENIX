#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>

#include "BSL/integral_types.h"
#include "PHX/types/queue_type.h"

namespace PHX
{
	class QueueFamilyIndices
	{
	public:

		static constexpr u32 INVALID_INDEX = U32_MAX;

		struct IndexPair
		{
			u32 queueFamilyIndex;
			u32 queueIndex; // Index of the queue within the queue family
		};

		QueueFamilyIndices();
		~QueueFamilyIndices();
		QueueFamilyIndices(const QueueFamilyIndices& other);
		QueueFamilyIndices(QueueFamilyIndices&& other) noexcept;
		QueueFamilyIndices& operator=(const QueueFamilyIndices& other);

		void SetIndices(QUEUE_TYPE type, u32 familyIndex, u32 queueIndex);

		u32 GetQueueIndex(QUEUE_TYPE type) const;
		u32 GetFamilyIndex(QUEUE_TYPE type) const;

		bool IsValid(IndexPair indexPair) const;
		bool IsComplete();

	private:

		std::unordered_map<QUEUE_TYPE, IndexPair> queueFamilies;
	};

	// Global helpers
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

	// Returns true for all queue types that require synchronization (i.e. all except PRESENT)
	bool NeedsSynchronization(QUEUE_TYPE type);

	const char* GetQueueTypeName(QUEUE_TYPE type);
}