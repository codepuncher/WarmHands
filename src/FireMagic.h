#pragma once

namespace RE
{
	class EffectSetting;
	class MagicItem;
}

namespace WarmHands::FireMagic
{
	[[nodiscard]] bool CountsAsFire(const RE::EffectSetting* effect);

	[[nodiscard]] bool IsFireSpell(const RE::MagicItem* item);
}
