#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>

#include "PHX/types/basic_types.h"
#include "PHX/types/queue_type.h"

namespace PHX
{
	class QueueFamilyIndices
	{
	public:

		static constexpr u32 INVALID_INDEX = U32_MAX;

		QueueFamilyIndices();
		~QueueFamilyIndices();
		QueueFamilyIndices(const QueueFamilyIndices& other);
		QueueFamilyIndices(QueueFamilyIndices&& other) noexcept;
		QueueFamilyIndices& operator=(const QueueFamilyIndices& other);

		void SetIndex(QUEUE_TYPE type, u32 index);
		u32 GetIndex(QUEUE_TYPE type) const;

		bool IsValid(u32 index) const;
		bool IsComplete();

	private:

		std::unordered_map<QUEUE_TYPE, u32> queueFamilies;
	};

	// Global function
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
}