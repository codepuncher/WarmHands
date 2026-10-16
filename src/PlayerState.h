#pragma once

#include "IdleDecision.h"
#include "WarmthSource.h"

namespace RE
{
	class PlayerCharacter;
}

namespace WarmHands::PlayerState
{
	[[nodiscard]] PlayerMagicState ReadMagicState(RE::PlayerCharacter& player);
	[[nodiscard]] bool             IsStandingStill(RE::PlayerCharacter& player);
	[[nodiscard]] IdleInputs       ReadIdleInputs(RE::PlayerCharacter& player);
}
