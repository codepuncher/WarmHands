#pragma once

#include "Settings.h"

namespace WarmHands::Config
{
	[[nodiscard]] Settings LoadSettings();
	[[nodiscard]] bool     SaveSettings(const Settings& settings);

	[[nodiscard]] Settings Current();
	[[nodiscard]] Settings Active();
	void                   SetCurrent(const Settings& settings);

	void ApplyLogLevel(const Settings& settings);
}
