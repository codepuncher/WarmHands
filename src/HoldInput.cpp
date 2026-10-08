#include "PCH.h"

#include "HoldInput.h"
#include "KeyNames.h"
#include "Ticker.h"

#include <ranges>

namespace
{
	using Keyboard = RE::BSKeyboardDevice::Key;
	using Gamepad = RE::BSWin32GamepadDevice::Key;

	constexpr std::array<std::uint32_t, 53> kLibraryKeyboardCodes{
		Keyboard::kQ,
		Keyboard::kW,
		Keyboard::kE,
		Keyboard::kR,
		Keyboard::kT,
		Keyboard::kY,
		Keyboard::kU,
		Keyboard::kI,
		Keyboard::kO,
		Keyboard::kP,
		Keyboard::kA,
		Keyboard::kS,
		Keyboard::kD,
		Keyboard::kF,
		Keyboard::kG,
		Keyboard::kH,
		Keyboard::kJ,
		Keyboard::kK,
		Keyboard::kL,
		Keyboard::kZ,
		Keyboard::kX,
		Keyboard::kC,
		Keyboard::kV,
		Keyboard::kB,
		Keyboard::kN,
		Keyboard::kM,
		Keyboard::kNum1,
		Keyboard::kNum2,
		Keyboard::kNum3,
		Keyboard::kNum4,
		Keyboard::kNum5,
		Keyboard::kNum6,
		Keyboard::kNum7,
		Keyboard::kNum8,
		Keyboard::kNum9,
		Keyboard::kNum0,
		Keyboard::kF1,
		Keyboard::kF2,
		Keyboard::kF3,
		Keyboard::kF4,
		Keyboard::kF5,
		Keyboard::kF6,
		Keyboard::kF7,
		Keyboard::kF8,
		Keyboard::kF9,
		Keyboard::kF10,
		Keyboard::kF11,
		Keyboard::kF12,
		Keyboard::kSpacebar,
		Keyboard::kTab,
		Keyboard::kLeftShift,
		Keyboard::kLeftControl,
		Keyboard::kLeftAlt,
	};

	constexpr std::array<std::uint32_t, 16> kLibraryGamepadCodes{
		Gamepad::kA,
		Gamepad::kB,
		Gamepad::kX,
		Gamepad::kY,
		Gamepad::kLeftShoulder,
		Gamepad::kRightShoulder,
		Gamepad::kLeftTrigger,
		Gamepad::kRightTrigger,
		Gamepad::kLeftThumb,
		Gamepad::kRightThumb,
		Gamepad::kStart,
		Gamepad::kBack,
		Gamepad::kUp,
		Gamepad::kDown,
		Gamepad::kLeft,
		Gamepad::kRight,
	};

	static_assert(std::ranges::equal(WarmHands::kKeyboardKeys | std::views::values, kLibraryKeyboardCodes));
	static_assert(std::ranges::equal(WarmHands::kGamepadButtons | std::views::values, kLibraryGamepadCodes));

	using ButtonEventPtr = std::unique_ptr<RE::ButtonEvent, void (*)(RE::ButtonEvent*)>;

	ButtonEventPtr MakeButtonEvent(RE::INPUT_DEVICE device, const RE::BSFixedString& userEvent, std::uint32_t keyCode, float value, float heldSeconds)
	{
		return {
			RE::ButtonEvent::Create(device, userEvent, keyCode, value, heldSeconds),
			[](RE::ButtonEvent* event) {
				event->~ButtonEvent();
				RE::free(event);
			}
		};
	}

	void ReplayShortPress(RE::BSTEventSource<RE::InputEvent*>* source, RE::INPUT_DEVICE device, const RE::BSFixedString& userEvent, std::uint32_t keyCode, float heldSeconds)
	{
		auto* menuControls = RE::MenuControls::GetSingleton();
		auto* playerControls = RE::PlayerControls::GetSingleton();
		if (!menuControls || !playerControls || userEvent.empty()) {
			return;
		}
		for (const auto& [value, held] : { std::pair{ 1.0F, 0.0F }, std::pair{ 0.0F, heldSeconds } }) {
			const auto event = MakeButtonEvent(device, userEvent, keyCode, value, held);
			if (!event) {
				logger::error("Could not allocate a replayed '{}' event", userEvent.c_str());
				return;
			}
			RE::InputEvent* const chain = event.get();
			static_cast<RE::BSTEventSink<RE::InputEvent*>*>(menuControls)->ProcessEvent(&chain, source);
			static_cast<RE::BSTEventSink<RE::InputEvent*>*>(playerControls)->ProcessEvent(&chain, source);
		}
	}
}

WarmHands::HoldInput* WarmHands::HoldInput::GetSingleton()
{
	static HoldInput singleton;
	return &singleton;
}

void WarmHands::HoldInput::Configure(const Settings& settings)
{
	enabled_ = settings.holdToWarmHands;
	holdDuration_ = settings.holdDuration;

	const auto keyboard = KeyboardKeyCode(settings.holdKeyboardKey);
	const auto gamepad = GamepadButtonCode(settings.holdGamepadButton);
	bindings_[0].device = RE::INPUT_DEVICE::kKeyboard;
	bindings_[0].keyCode = keyboard.value_or(0);
	bindings_[1].device = RE::INPUT_DEVICE::kGamepad;
	bindings_[1].keyCode = gamepad.value_or(0);

	for (const auto& binding : bindings_) {
		logger::info("Hold binding {:#x} on device {}", binding.keyCode, static_cast<int>(binding.device));
	}
}

bool WarmHands::HoldInput::ProcessButton(const RE::ButtonEvent& button, Binding& binding, RE::BSTEventSource<RE::InputEvent*>* source) const
{
	HoldOutcome outcome = HoldOutcome::kPass;
	if (button.IsDown()) {
		const bool armed = enabled_ && Ticker::HoldArmed();
		logger::debug("Hold key {:#x} down, armed={}", binding.keyCode, armed);
		outcome = binding.tracker.OnDown(armed, Ticker::IsIdlePlaying());
	} else if (button.IsHeld()) {
		outcome = binding.tracker.OnHeld(button.HeldDuration(), holdDuration_);
	} else if (button.IsUp()) {
		outcome = binding.tracker.OnUp(button.HeldDuration(), holdDuration_);
	}

	switch (outcome) {
	case HoldOutcome::kStartIdle:
		Ticker::StartIdleFromHold();
		return true;
	case HoldOutcome::kStopIdle:
		Ticker::DismissIdle();
		return true;
	case HoldOutcome::kReplayShortPress:
		ReplayShortPress(source, binding.device, button.QUserEvent(), binding.keyCode, button.HeldDuration());
		return true;
	case HoldOutcome::kBlock:
		return true;
	case HoldOutcome::kPass:
		return false;
	}
	return false;
}

namespace
{
	bool IsMovementInput(const RE::InputEvent& event)
	{
		const auto* userEvents = RE::UserEvents::GetSingleton();
		if (!userEvents) {
			return false;
		}
		const auto& name = event.QUserEvent();
		if (const auto* stick = event.AsThumbstickEvent()) {
			return name == userEvents->move && (stick->xValue != 0.0F || stick->yValue != 0.0F);
		}
		const auto* button = event.AsButtonEvent();
		if (!button || !button->IsPressed()) {
			return false;
		}
		return name == userEvents->forward || name == userEvents->back || name == userEvents->strafeLeft || name == userEvents->strafeRight;
	}
}

RE::BSEventNotifyControl WarmHands::HoldInput::ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>* a_source)
{
	if (!a_events) {
		return RE::BSEventNotifyControl::kContinue;
	}
	for (auto* event = *a_events; event; event = event->next) {
		if (IsMovementInput(*event)) {
			Ticker::OnMovementInput();
		}
		auto* button = event->AsButtonEvent();
		if (!button) {
			continue;
		}
		for (auto& binding : bindings_) {
			if (binding.keyCode == 0 || button->GetDevice() != binding.device || button->GetIDCode() != binding.keyCode) {
				continue;
			}
			if (ProcessButton(*button, binding, a_source)) {
				button->GetRuntimeData().value = 0.0F;
				button->GetRuntimeData().heldDownSecs = 0.0F;
			}
		}
	}
	return RE::BSEventNotifyControl::kContinue;
}
