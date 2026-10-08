#include "PCH.h"

#include "Config.h"
#include "HeatSource.h"
#include "IdleDecision.h"
#include "PlayerState.h"
#include "Ticker.h"        // NOLINT(readability-duplicate-include): clang-tidy 21 false positive on Linux clang-cl
#include "WarmthSource.h"  // NOLINT(readability-duplicate-include): clang-tidy 21 false positive on Linux clang-cl

namespace
{
	constexpr auto       kTickPeriod = 1s;
	constexpr float      kTickSeconds = std::chrono::duration<float>(kTickPeriod).count();
	constexpr RE::FormID kIdleWarmHandsStanding = 0x0E8642;

	struct State
	{
		float                    stillSeconds{ 0.0F };
		bool                     warmedByFireMagic{ false };
		bool                     idlePlaying{ false };
		bool                     idleDismissed{ false };
		bool                     idleBaselineKnown{ false };
		const RE::TESIdleForm*   idleBaseline{ nullptr };
		bool                     leftoversPending{ true };
		const RE::TESObjectCELL* cell{ nullptr };
	};

	State& GetState()
	{
		static State state;
		return state;
	}

	RE::PlayerCharacter* LoadedPlayer()
	{
		auto* ui = RE::UI::GetSingleton();
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!ui || ui->GameIsPaused() || !player || !player->Is3DLoaded()) {
			return nullptr;
		}
		return player;
	}

	bool UpdateWarmth(RE::PlayerCharacter& player, const WarmHands::Settings& settings)
	{
		const WarmHands::WarmthRules rules{
			.requireActiveCast = settings.requireActiveCast,
			.requireBothHands = settings.requireBothHands,
			.warmFromActiveFireEffects = settings.warmFromActiveFireEffects,
		};
		const bool warmed = settings.enableFireMagicWarmth &&
		                    WarmHands::IsWarmedByFireMagic(WarmHands::PlayerState::ReadMagicState(player), rules);
		WarmHands::HeatSource::Update(player, warmed);
		return warmed;
	}

	WarmHands::IdleInputs GatherIdleInputs(State& state, RE::PlayerCharacter& player, const WarmHands::Settings& settings)
	{
		auto       inputs = WarmHands::PlayerState::ReadIdleInputs(player);
		const bool idleEnabled = settings.autoWarmHands || settings.holdToWarmHands;
		const bool measure = idleEnabled || settings.showActivationRange;
		const auto heat = measure ? WarmHands::HeatSource::FindHeat(player, settings, state.idlePlaying) : WarmHands::HeatSource::NearHeat{};
		inputs.nearHeat = idleEnabled && heat.any;
		inputs.autoHeat = heat.autoEnabled;
		inputs.stillSeconds = state.stillSeconds;
		inputs.idleDelaySeconds = static_cast<float>(settings.idleDelay);
		inputs.warmedByFireMagic = state.warmedByFireMagic;
		return inputs;
	}

	bool StartIdle(RE::PlayerCharacter& player)
	{
		static auto* idle = RE::TESForm::LookupByID<RE::TESIdleForm>(kIdleWarmHandsStanding);
		auto*        process = player.GetActorRuntimeData().currentProcess;
		if (!idle || !process) {
			return false;
		}
		const bool started = process->PlayIdle(&player, idle, nullptr);
		logger::debug("PlayIdle(IdleWarmHandsStanding) returned {}", started);
		return started;
	}

	const RE::TESIdleForm* LastIdlePlayed(RE::PlayerCharacter& player)
	{
		const auto* process = player.GetActorRuntimeData().currentProcess;
		const auto* middleHigh = process ? process->middleHigh : nullptr;
		return middleHigh ? middleHigh->lastIdlePlayed : nullptr;
	}

	void ForgetReplacedIdle(State& state, RE::PlayerCharacter& player)
	{
		if (!state.idlePlaying) {
			return;
		}
		const auto* last = LastIdlePlayed(player);
		if (!state.idleBaselineKnown) {
			state.idleBaseline = last;
			state.idleBaselineKnown = true;
			return;
		}
		if (last != state.idleBaseline) {
			state.idlePlaying = false;
		}
	}

	void BeginIdle(State& state, RE::PlayerCharacter& player)
	{
		state.idlePlaying = StartIdle(player);
		state.idleBaselineKnown = false;
	}

	void EndIdle(State& state, RE::PlayerCharacter& player)
	{
		if (!player.NotifyAnimationGraph("IdleForceDefaultState")) {
			logger::debug("IdleForceDefaultState was not accepted");
		}
		state.idlePlaying = false;
	}

	void UpdateIdle(State& state, RE::PlayerCharacter& player, const WarmHands::Settings& settings)
	{
		ForgetReplacedIdle(state, player);
		const bool sameCell = player.GetParentCell() == state.cell;
		state.cell = player.GetParentCell();
		state.stillSeconds = sameCell && WarmHands::PlayerState::IsStandingStill(player) ? state.stillSeconds + kTickSeconds : 0.0F;

		const auto inputs = GatherIdleInputs(state, player, settings);
		state.idleDismissed = WarmHands::KeepDismissed(state.idleDismissed, inputs);
		const auto action = WarmHands::DecideIdle(inputs, state.idlePlaying, settings.autoWarmHands && !state.idleDismissed);
		logger::debug("Idle: nearHeat={} still={:.0f}s combat={} sneak={} mounted={} drawn={} torch={} warmed={} busy={} playing={} -> {}",
			inputs.nearHeat, inputs.stillSeconds, inputs.inCombat, inputs.sneaking, inputs.mounted, inputs.handsDrawn,
			inputs.holdingTorch, inputs.warmedByFireMagic, inputs.busy, state.idlePlaying, static_cast<int>(action));
		switch (action) {
		case WarmHands::IdleAction::kStart:
			BeginIdle(state, player);
			break;
		case WarmHands::IdleAction::kStop:
			EndIdle(state, player);
			break;
		case WarmHands::IdleAction::kForget:
			state.idlePlaying = false;
			state.idleDismissed = true;
			break;
		case WarmHands::IdleAction::kNone:
			break;
		}
	}

	void Update()
	{
		auto* player = LoadedPlayer();
		if (!player) {
			return;
		}
		auto& state = GetState();
		if (state.leftoversPending) {
			WarmHands::HeatSource::RemoveLeftovers(*player);
			state.leftoversPending = false;
		}
		const auto settings = WarmHands::Config::Active();
		state.warmedByFireMagic = UpdateWarmth(*player, settings);
		UpdateIdle(state, *player, settings);
	}
}

void WarmHands::Ticker::Start()
{
	const auto* tasks = SKSE::GetTaskInterface();
	if (!tasks) {
		logger::error("SKSE task interface unavailable; Warm Hands disabled");
		return;
	}
	static std::jthread thread{ [tasks](const std::stop_token& stop) {
		while (!stop.stop_requested()) {
			std::this_thread::sleep_for(kTickPeriod);
			tasks->AddTask(Update);
		}
	} };
}

void WarmHands::Ticker::Reset()
{
	auto& state = GetState();
	state.stillSeconds = 0.0F;
	state.warmedByFireMagic = false;
	state.idlePlaying = false;
	state.idleDismissed = false;
	state.leftoversPending = true;
}

bool WarmHands::Ticker::HoldArmed()
{
	auto* player = LoadedPlayer();
	if (!player) {
		return false;
	}
	auto&      state = GetState();
	const auto settings = Config::Active();
	auto       inputs = GatherIdleInputs(state, *player, settings);
	inputs.stillSeconds = PlayerState::IsStandingStill(*player) ? std::max(state.stillSeconds, kTickSeconds) : 0.0F;
	return settings.holdToWarmHands && CanStartOnHold(inputs);
}

void WarmHands::Ticker::StartIdleFromHold()
{
	auto* player = LoadedPlayer();
	auto& state = GetState();
	if (!player || state.idlePlaying || !HoldArmed()) {
		return;
	}
	state.idleDismissed = false;
	BeginIdle(state, *player);
}

bool WarmHands::Ticker::IsIdlePlaying()
{
	return GetState().idlePlaying;
}

void WarmHands::Ticker::DismissIdle()
{
	auto& state = GetState();
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!state.idlePlaying || !player) {
		return;
	}
	logger::debug("Tap dismissed the warm hands idle");
	EndIdle(state, *player);
	state.idleDismissed = true;
}

void WarmHands::Ticker::OnMovementInput()
{
	auto& state = GetState();
	state.stillSeconds = 0.0F;
	state.idleDismissed = false;
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!state.idlePlaying || !player) {
		return;
	}
	logger::debug("Movement ended the warm hands idle");
	EndIdle(state, *player);
}
