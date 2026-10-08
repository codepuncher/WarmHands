#pragma once

#include "HoldTracker.h"
#include "Settings.h"

namespace WarmHands
{
	class HoldInput final : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		static HoldInput* GetSingleton();

		HoldInput(const HoldInput&) = delete;
		HoldInput(HoldInput&&) = delete;
		HoldInput& operator=(const HoldInput&) = delete;
		HoldInput& operator=(HoldInput&&) = delete;
		~HoldInput() override = default;

		void Configure(const Settings& settings);

		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>* a_source) override;

	private:
		HoldInput() = default;

		struct Binding
		{
			RE::INPUT_DEVICE device{ RE::INPUT_DEVICE::kNone };
			std::uint32_t    keyCode{ 0 };
			HoldTracker      tracker;
		};

		[[nodiscard]] bool ProcessButton(const RE::ButtonEvent& button, Binding& binding, RE::BSTEventSource<RE::InputEvent*>* source) const;

		std::array<Binding, 2> bindings_{};
		bool                   enabled_{ false };
		float                  holdDuration_{ 0.5F };
	};
}
