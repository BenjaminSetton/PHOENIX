
#include "global_settings.h"

#include "utils/logger.h"

namespace PHX
{
	const Settings& GlobalSettings::GetSettings() const
	{
		return m_settings;
	}

	void GlobalSettings::SetSettings(const Settings& settings)
	{
		if (m_isSet)
		{
			return;
		}

		m_settings = settings;
		m_isSet = true;
	}

	GlobalSettings::GlobalSettings() : m_settings(), m_isSet(false)
	{
	}

	//---------------------------------------------------------------------

	const Settings& GetSettings()
	{
		return GlobalSettings::Get().GetSettings();
	}
}