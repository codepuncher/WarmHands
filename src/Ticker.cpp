#include "PCH.h"

#include "Config.h"
#include "GameEvents.h"
#include "HeatSource.h"
#include "IdleDecision.h"
#include "PlayerState.h"
#include "Ticker.h"        // NOLINT(readability-duplicate-include): clang-tidy 21 false positive on Linux clang-cl
#include "WarmthSource.h"  // NOLINT(readability-duplicate-include): clang-tidy 21 false positive on Linux clang-cl

namespace
{
	using Clock = std::chrono::steady_clock;

	constexpr auto       kOverlayPeriod = 1s;
	constexpr auto       kTransitionPeriod = 250ms;
	constexpr auto       kMovingPeriod = 1s;
	constexpr auto       kRetryPeriod = 1s;
	constexpr auto       kLookSettle = 500ms;
	constexpr RE::FormID kIdleWarmHandsStanding = 0x0E8642;

	void Evaluate();

	class Scheduler
	{
	public:
		void Request(Clock::duration delay)
		{
			bool sooner = false;
			{
				const std::scoped_lock lock{ mutex_ };
				const auto             at = Clock::now() + delay;
				sooner = !checkAt_ || at < *checkAt_;
				checkAt_ = sooner ? at : checkAt_;
			}
			if (sooner) {
				wake_.notify_one();
			}
		}

		void SetStillAt(std::optional<Clock::time_point> at)
		{
			bool sooner = false;
			{
				const std::scoped_lock lock{ mutex_ };
				sooner = at && (!stillAt_ || *at < *stillAt_);
				stillAt_ = at;
			}
			if (sooner) {
				wake_.notify_one();
			}
		}

		void Run(const std::stop_token& stop, const SKSE::TaskInterface& tasks)
		{
			std::unique_lock lock{ mutex_ };
			while (!stop.stop_requested()) {
				const auto next = Earliest();
				if (!next) {
					wake_.wait(lock, stop, [this] { return Earliest().has_value(); });
					continue;
				}
				if (Clock::now() < *next) {
					wake_.wait_until(lock, stop, *next, [this, next] { return Earliest() != next; });
					continue;
				}
				const auto now = Clock::now();
				if (checkAt_ && *checkAt_ <= now) {
					checkAt_.reset();
				}
				if (stillAt_ && *stillAt_ <= now) {
					stillAt_.reset();
				}
				if (queued_) {
					continue;
				}
				queued_ = true;
				lock.unlock();
				tasks.AddTask(Evaluate);
				lock.lock();
			}
		}

		void Started()
		{
			const std::scoped_lock lock{ mutex_ };
			queued_ = false;
		}

	private:
		[[nodiscard]] std::optional<Clock::time_point> Earliest() const
		{
			if (checkAt_ && stillAt_) {
				return std::min(*checkAt_, *stillAt_);
			}
			return checkAt_ ? checkAt_ : stillAt_;
		}

		std::mutex                       mutex_;
		std::condition_variable_any      wake_;
		std::optional<Clock::time_point> checkAt_;
		std::optional<Clock::time_point> stillAt_;
		bool                             queued_{ false };
	};

	Scheduler& GetScheduler()
	{
		static Scheduler scheduler;
		return scheduler;
	}

	struct State
	{
		Clock::time_point        stillSince{ Clock::now() };
		Clock::duration          idleDelay{ std::chrono::seconds{ WarmHands::kDefaultIdleDelay } };
		Clock::time_point        lookSettledAt;
		Clock::time_point        idleStartedAt;
		const RE::TESIdleForm*   idleBaseline{ nullptr };
		const RE::TESObjectCELL* cell{ nullptr };
		bool                     autoActive{ false };
		bool                     warmedByFireMagic{ false };
		bool                     heatNearby{ false };
		bool                     idlePlaying{ false };
		bool                     idleDismissed{ false };
		bool                     idleBaselineKnown{ false };
		bool                     wasBlocked{ false };
		bool                     leftoversPending{ true };
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

	float StillSeconds(const State& state, RE::PlayerCharacter& player)
	{
		if (!WarmHands::PlayerState::IsStandingStill(player)) {
			return 0.0F;
		}
		// Zero means moving, so a timer restarted this instant still reads as standing.
		return std::max(std::chrono::duration<float>(Clock::now() - state.stillSince).count(), std::numeric_limits<float>::min());
	}

	WarmHands::IdleInputs GatherIdleInputs(State& state, RE::PlayerCharacter& player, const WarmHands::Settings& settings)
	{
		auto       inputs = WarmHands::PlayerState::ReadIdleInputs(player);
		const bool idleEnabled = settings.autoWarmHands || settings.holdToWarmHands;
		const bool measure = idleEnabled || settings.showActivationRange;
		const auto heat = measure ? WarmHands::HeatSource::FindHeat(player, settings, state.idlePlaying) : WarmHands::HeatSource::NearHeat{};
		inputs.nearHeat = idleEnabled && heat.any;
		state.heatNearby = idleEnabled && heat.nearby;
		inputs.autoHeat = heat.autoEnabled;
		inputs.stillSeconds = StillSeconds(state, player);
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
		if (!state.idleBaselineKnown && Clock::now() < state.idleStartedAt + kTransitionPeriod) {
			return;
		}
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
		state.idleStartedAt = Clock::now();
		// The next evaluation records the idle baseline, or retries a start the graph refused.
		GetScheduler().Request(state.idlePlaying ? kTransitionPeriod : kRetryPeriod);
	}

	void EndIdle(State& state, RE::PlayerCharacter& player)
	{
		if (!player.NotifyAnimationGraph("IdleForceDefaultState")) {
			logger::debug("IdleForceDefaultState was not accepted");
		}
		state.idlePlaying = false;
	}

	WarmHands::IdleInputs UpdateIdle(State& state, RE::PlayerCharacter& player, const WarmHands::Settings& settings)
	{
		ForgetReplacedIdle(state, player);
		const bool sameCell = player.GetParentCell() == state.cell;
		state.cell = player.GetParentCell();
		if (!sameCell || !WarmHands::PlayerState::IsStandingStill(player)) {
			state.stillSince = Clock::now();
		}

		auto       inputs = GatherIdleInputs(state, player, settings);
		const bool blocked = WarmHands::IsBlocked(inputs);
		if (state.wasBlocked && !blocked) {
			state.stillSince = Clock::now();
			inputs.stillSeconds = StillSeconds(state, player);
		}
		state.wasBlocked = blocked;
		state.idleDismissed = WarmHands::KeepDismissed(state.idleDismissed, inputs);
		const bool lookSettled = Clock::now() >= state.lookSettledAt;
		const auto action = WarmHands::DecideIdle(inputs, state.idlePlaying, settings.autoWarmHands && !state.idleDismissed && lookSettled);
		logger::debug("Idle: nearHeat={} still={:.1f}s combat={} sneak={} mounted={} drawn={} torch={} warmed={} busy={} playing={} -> {}",
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
		return inputs;
	}

	bool IsTransient(RE::MagicCaster::State casterState)
	{
		using CasterState = RE::MagicCaster::State;
		switch (casterState) {
		case CasterState::kNone:
		case CasterState::kReady:
		case CasterState::kCharging:
		case CasterState::kCasting:
			return false;
		default:
			return true;
		}
	}

	bool InTransition(RE::PlayerCharacter& player)
	{
		using WeaponState = RE::WEAPON_STATE;
		const auto weapon = player.AsActorState()->GetWeaponState();
		if (weapon != WeaponState::kSheathed && weapon != WeaponState::kDrawn) {
			return true;
		}
		using Source = RE::MagicSystem::CastingSource;
		return std::ranges::any_of(std::array{ Source::kLeftHand, Source::kRightHand }, [&player](Source source) {
			const auto* caster = player.GetMagicCaster(source);
			return caster && IsTransient(caster->state.get());
		});
	}

	Clock::time_point StillDeadline(const State& state)
	{
		return std::max(state.stillSince + state.idleDelay, state.lookSettledAt);
	}

	void ArmStillCheck(const State& state)
	{
		const bool pending = state.autoActive && !state.idlePlaying && !state.idleDismissed;
		GetScheduler().SetStillAt(pending ? std::optional{ StillDeadline(state) } : std::nullopt);
	}

	void ScheduleFollowUp(const State& state, RE::PlayerCharacter& player, const WarmHands::Settings& settings, const WarmHands::IdleInputs& inputs)
	{
		if (settings.showActivationRange) {
			GetScheduler().Request(kOverlayPeriod);
		}
		if (InTransition(player)) {
			GetScheduler().Request(kTransitionPeriod);
		}
		// Staggers, script idles and fires being deleted end ours without an event.
		if (state.idlePlaying) {
			GetScheduler().Request(state.idleBaselineKnown ? kRetryPeriod : kTransitionPeriod);
		}
		const bool moving = inputs.stillSeconds <= 0.0F;
		// Auto-Move and knockback move the player without any input events.
		if (moving && (state.warmedByFireMagic || state.autoActive)) {
			GetScheduler().Request(kMovingPeriod);
		}
		if (moving) {
			return;
		}
		// The furniture, combat and sneak key events arrive before the state they report clears.
		const bool blocked = inputs.busy || inputs.mounted || inputs.inCombat || inputs.sneaking;
		if (blocked && state.autoActive && state.heatNearby) {
			GetScheduler().Request(kRetryPeriod);
		}
		if (!inputs.autoHeat || !WarmHands::CanStartIdle(inputs)) {
			GetScheduler().SetStillAt(std::nullopt);
			return;
		}
		if (Clock::now() < StillDeadline(state)) {
			ArmStillCheck(state);
		}
	}

	void Evaluate()
	{
		GetScheduler().Started();
		auto& state = GetState();
		auto* player = LoadedPlayer();
		if (!player) {
			state.wasBlocked = true;
			GetScheduler().Request(kRetryPeriod);
			return;
		}
		WarmHands::GameEvents::GetSingleton()->AttachToPlayer();
		if (state.leftoversPending) {
			WarmHands::HeatSource::RemoveLeftovers(*player);
			state.leftoversPending = false;
		}
		const auto settings = WarmHands::Config::Active();
		state.autoActive = settings.autoWarmHands;
		state.idleDelay = std::chrono::seconds{ settings.idleDelay };
		state.warmedByFireMagic = UpdateWarmth(*player, settings);
		const auto inputs = UpdateIdle(state, *player, settings);
		ScheduleFollowUp(state, *player, settings, inputs);
	}
}

void WarmHands::Ticker::Start()
{
	const auto* tasks = SKSE::GetTaskInterface();
	if (!tasks) {
		logger::error("SKSE task interface unavailable; Warm Hands disabled");
		return;
	}
	auto&               scheduler = GetScheduler();
	static std::jthread thread{ [&scheduler, tasks](const std::stop_token& stop) { scheduler.Run(stop, *tasks); } };
	RequestCheck();
}

void WarmHands::Ticker::RequestCheck()
{
	GetScheduler().Request(Clock::duration::zero());
}

void WarmHands::Ticker::Reset()
{
	auto& state = GetState();
	state.stillSince = Clock::now();
	state.warmedByFireMagic = false;
	state.idlePlaying = false;
	state.idleDismissed = false;
	state.wasBlocked = false;
	state.leftoversPending = true;
	RequestCheck();
}

bool WarmHands::Ticker::HoldArmed()
{
	auto* player = LoadedPlayer();
	if (!player) {
		return false;
	}
	auto&      state = GetState();
	const auto settings = Config::Active();
	const auto inputs = GatherIdleInputs(state, *player, settings);
	return settings.holdToWarmHands && CanStartIdle(inputs);
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
	state.stillSince = Clock::now();
	state.idleDismissed = false;
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		return;
	}
	if (state.warmedByFireMagic) {
		HeatSource::Follow(*player);
		// Auto-Move keeps moving after its only input, so a later check has to see the movement.
		GetScheduler().Request(kMovingPeriod);
	}
	if (state.idlePlaying) {
		logger::debug("Movement ended the warm hands idle");
		EndIdle(state, *player);
	}
	ArmStillCheck(state);
}

void WarmHands::Ticker::OnLookInput()
{
	auto& state = GetState();
	if (!state.heatNearby || state.wasBlocked) {
		return;
	}
	state.lookSettledAt = Clock::now() + kLookSettle;
	ArmStillCheck(state);
}
