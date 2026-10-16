#pragma once

namespace WarmHands
{
	enum class HoldOutcome
	{
		kPass,
		kBlock,
		kStartIdle,
		kReplayShortPress,
		kStopIdle,
	};

	class HoldTracker
	{
	public:
		[[nodiscard]] constexpr HoldOutcome OnDown(bool armed, bool idlePlaying) noexcept
		{
			tracking_ = armed;
			fired_ = false;
			startedDuringIdle_ = idlePlaying;
			return armed ? HoldOutcome::kBlock : HoldOutcome::kPass;
		}

		[[nodiscard]] constexpr HoldOutcome OnHeld(float heldSeconds, float duration) noexcept
		{
			if (!tracking_) {
				return HoldOutcome::kPass;
			}
			if (fired_ || heldSeconds < duration) {
				return HoldOutcome::kBlock;
			}
			fired_ = true;
			return HoldOutcome::kStartIdle;
		}

		[[nodiscard]] constexpr HoldOutcome OnUp(float heldSeconds, float duration) noexcept
		{
			if (!tracking_) {
				return HoldOutcome::kPass;
			}
			tracking_ = false;
			if (fired_) {
				return HoldOutcome::kBlock;
			}
			if (heldSeconds >= duration) {
				return HoldOutcome::kStartIdle;
			}
			return startedDuringIdle_ ? HoldOutcome::kStopIdle : HoldOutcome::kReplayShortPress;
		}

	private:
		bool tracking_{ false };
		bool fired_{ false };
		bool startedDuringIdle_{ false };
	};
}
