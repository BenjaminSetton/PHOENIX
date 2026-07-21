#pragma once

#include <functional>
#include <vector>

#include "PHX/types/integral_types.h"

namespace PHX
{
	// Generic deferred notification system. Allows any system to schedule a
	// callback (lambda) to be invoked after a specified number of frames have
	// elapsed
	class DeferredCaller
	{
	public:

		static DeferredCaller& Get()
		{
			static DeferredCaller s_instance;
			return s_instance;
		}

		// Registers a callback to be invoked after frameDelay frames have passed.
		// A frameDelay of 0 means the callback fires on the next Update() call
		void Register(u32 frameDelay, std::function<void()> callback);

		// Advances the frame counter and fires any callbacks whose delay has elapsed
		void Update();

		// Fires all remaining callbacks immediately and clears the queue
		void Flush();

	private:

		DeferredCaller() = default;
		~DeferredCaller() = default;
		DeferredCaller(const DeferredCaller&) = delete;
		DeferredCaller& operator=(const DeferredCaller&) = delete;

		struct DeferredEntry
		{
			std::function<void()> callback;
			u64 targetFrame = 0;
		};

		std::vector<DeferredEntry> m_pending;
		u64 m_currentFrame = 0;
	};
}
