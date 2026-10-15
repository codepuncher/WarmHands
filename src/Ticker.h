#pragma once

namespace WarmHands::Ticker
{
	void Start();

	void Reset();

	void RequestCheck();

	[[nodiscard]] bool HoldArmed();

	void StartIdleFromHold();

	[[nodiscard]] bool IsIdlePlaying();

	void DismissIdle();

	void OnMovementInput();

	void OnLookInput();
}
