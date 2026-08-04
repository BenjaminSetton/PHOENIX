
#include "deferred_caller.h"

namespace PHX
{
	void DeferredCaller::Register(u32 frameDelay, std::function<void()> callback)
	{
		m_pending.push_back({ std::move(callback), m_currentFrame + frameDelay });
	}

	void DeferredCaller::Update()
	{
		m_currentFrame++;

		for (auto it = m_pending.begin(); it != m_pending.end(); )
		{
			if (m_currentFrame >= it->targetFrame)
			{
				it->callback();
				it = m_pending.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void DeferredCaller::Flush()
	{
		for (auto& entry : m_pending)
		{
			entry.callback();
		}
		m_pending.clear();
	}
}
