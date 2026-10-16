#pragma once

namespace WarmHands
{
	struct HandState
	{
		bool fireSpellEquipped{ false };
		bool casting{ false };
		bool twoHanded{ false };
	};

	struct PlayerMagicState
	{
		HandState left{};
		HandState right{};
		bool      handsDrawn{ false };
		bool      dualCasting{ false };
		bool      selfFireEffectActive{ false };
	};

	struct WarmthRules
	{
		bool requireActiveCast{ false };
		bool requireBothHands{ false };
		bool warmFromActiveFireEffects{ true };
	};

	[[nodiscard]] constexpr bool HandWarms(const HandState& hand, bool handsDrawn, const WarmthRules& rules) noexcept
	{
		if (!hand.fireSpellEquipped) {
			return false;
		}
		return rules.requireActiveCast ? hand.casting : handsDrawn;
	}

	[[nodiscard]] constexpr bool IsWarmedByFireMagic(const PlayerMagicState& state, const WarmthRules& rules) noexcept
	{
		if (rules.warmFromActiveFireEffects && state.selfFireEffectActive) {
			return true;
		}
		const bool left = HandWarms(state.left, state.handsDrawn, rules);
		const bool right = HandWarms(state.right, state.handsDrawn, rules);
		if (!rules.requireBothHands) {
			return left || right;
		}
		const bool twoHandedWarms = (left && state.left.twoHanded) || (right && state.right.twoHanded);
		const bool dualCastWarms = state.dualCasting && (left || right) && state.left.fireSpellEquipped && state.right.fireSpellEquipped;
		return twoHandedWarms || dualCastWarms || (left && right);
	}
}
