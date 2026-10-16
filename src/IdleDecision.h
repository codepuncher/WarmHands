#pragma once

#include "Settings.h"

namespace WarmHands
{
	struct IdleInputs
	{
		bool  nearHeat{ false };
		float stillSeconds{ 0.0F };
		bool  inCombat{ false };
		bool  sneaking{ false };
		bool  mounted{ false };
		bool  handsDrawn{ false };
		bool  holdingTorch{ false };
		bool  warmedByFireMagic{ false };
		bool  busy{ false };
		bool  autoHeat{ true };
		float idleDelaySeconds{ static_cast<float>(kDefaultIdleDelay) };
	};

	enum class IdleAction
	{
		kNone,
		kStart,
		kStop,
		kForget,
	};

	[[nodiscard]] constexpr bool IsBlocked(const IdleInputs& in) noexcept
	{
		return in.inCombat || in.sneaking || in.mounted || in.handsDrawn || in.holdingTorch || in.warmedByFireMagic || in.busy;
	}

	[[nodiscard]] constexpr bool CanStartIdle(const IdleInputs& in) noexcept
	{
		return in.nearHeat && in.stillSeconds > 0.0F && !IsBlocked(in);
	}

	[[nodiscard]] constexpr bool KeepDismissed(bool dismissed, const IdleInputs& in) noexcept
	{
		return dismissed && in.nearHeat && in.stillSeconds > 0.0F;
	}

	[[nodiscard]] constexpr IdleAction DecideIdle(const IdleInputs& in, bool idlePlaying, bool autoEnabled) noexcept
	{
		const bool canPlay = CanStartIdle(in);
		if (!idlePlaying) {
			const bool autoStart = autoEnabled && in.autoHeat && canPlay && in.stillSeconds >= in.idleDelaySeconds;
			return autoStart ? IdleAction::kStart : IdleAction::kNone;
		}
		if (canPlay) {
			return IdleAction::kNone;
		}
		const bool playerBrokeIdle = in.stillSeconds <= 0.0F || in.handsDrawn || in.busy;
		return playerBrokeIdle ? IdleAction::kForget : IdleAction::kStop;
	}
}
