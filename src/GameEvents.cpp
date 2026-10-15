#include "PCH.h"

#include "Config.h"
#include "GameEvents.h"
#include "HeatSource.h"
#include "KeyNames.h"
#include "Ticker.h"

namespace
{
	constexpr std::array<std::string_view, 10> kAnimationTags{
		"weaponDraw",
		"weaponSheathe",
		"WeapEquip_Out",
		"WeapEquip_OutMoving",
		"Unequip_Out",
		"Unequip_OutMoving",
		"BeginCastLeft",
		"BeginCastRight",
		"CastStop",
		"InterruptCast",
	};

	// AddEventSink only checks the pending list while the source is sending, which lets a duplicate in.
	bool IsWatching(RE::BSTEventSource<RE::BSAnimationGraphEvent>& source, RE::BSTEventSink<RE::BSAnimationGraphEvent>* sink)
	{
		const RE::BSSpinLockGuard lock{ source.lock };
		const auto                contains = [sink](const auto& sinks) { return std::ranges::find(sinks, sink) != sinks.end(); };
		return contains(source.sinks) || contains(source.pendingRegisters);
	}

	bool IsPlayer(const RE::TESObjectREFR* ref)
	{
		return ref && ref->IsPlayerRef();
	}

	struct PendingLoads
	{
		std::mutex                               mutex;
		std::vector<std::pair<RE::FormID, bool>> loads;
	};

	PendingLoads& GetPendingLoads()
	{
		static PendingLoads pending;
		return pending;
	}

	void CheckPendingLoads()
	{
		std::vector<std::pair<RE::FormID, bool>> loads;
		{
			auto&                  pending = GetPendingLoads();
			const std::scoped_lock lock{ pending.mutex };
			loads.swap(pending.loads);
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		const auto settings = WarmHands::Config::Active();
		bool       relevant = false;
		for (const auto& [formID, loaded] : loads) {
			const auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(formID);
			if (!ref || !WarmHands::HeatSource::IsHeatSourceNear(*player, *ref, settings)) {
				continue;
			}
			logger::debug("Heat source {:08X} {}", formID, loaded ? "loaded" : "unloaded");
			relevant = true;
		}
		if (relevant) {
			WarmHands::Ticker::RequestCheck();
		}
	}

	RE::BSEventNotifyControl CheckIf(bool relevant)
	{
		if (relevant) {
			WarmHands::Ticker::RequestCheck();
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}

WarmHands::GameEvents* WarmHands::GameEvents::GetSingleton()
{
	static GameEvents singleton;
	return &singleton;
}

void WarmHands::GameEvents::Register()
{
	auto* scripts = RE::ScriptEventSourceHolder::GetSingleton();
	auto* ui = RE::UI::GetSingleton();
	if (!scripts || !ui) {
		logger::error("Game event sources unavailable; Warm Hands only reacts to input");
		return;
	}
	scripts->AddEventSink<RE::TESEquipEvent>(this);
	scripts->AddEventSink<RE::TESActiveEffectApplyRemoveEvent>(this);
	scripts->AddEventSink<RE::TESCombatEvent>(this);
	scripts->AddEventSink<RE::TESFurnitureEvent>(this);
	scripts->AddEventSink<RE::TESObjectLoadedEvent>(this);
	ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
}

void WarmHands::GameEvents::AttachToPlayer()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		return;
	}
	RE::BSAnimationGraphManagerPtr manager;
	if (!player->GetAnimationGraphManager(manager) || !manager) {
		return;
	}
	// CommonLib's AddAnimationGraphEventSink attaches only to the first graph, which misses first person.
	for (const auto& graph : manager->graphs) {
		auto* source = graph ? graph->GetEventSource<RE::BSAnimationGraphEvent>() : nullptr;
		if (source && !IsWatching(*source, this)) {
			source->AddEventSink(this);
		}
	}
}

RE::BSEventNotifyControl WarmHands::GameEvents::ProcessEvent(const RE::TESEquipEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::TESEquipEvent>* a_source)
{
	return CheckIf(a_event && IsPlayer(a_event->actor.get()));
}

RE::BSEventNotifyControl WarmHands::GameEvents::ProcessEvent(const RE::TESActiveEffectApplyRemoveEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>* a_source)
{
	return CheckIf(a_event && IsPlayer(a_event->target.get()));
}

RE::BSEventNotifyControl WarmHands::GameEvents::ProcessEvent(const RE::TESCombatEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::TESCombatEvent>* a_source)
{
	return CheckIf(a_event && (IsPlayer(a_event->targetActor.get()) || IsPlayer(a_event->actor.get())));
}

RE::BSEventNotifyControl WarmHands::GameEvents::ProcessEvent(const RE::TESFurnitureEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::TESFurnitureEvent>* a_source)
{
	return CheckIf(a_event && IsPlayer(a_event->actor.get()));
}

RE::BSEventNotifyControl WarmHands::GameEvents::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_source)
{
	const auto* tasks = SKSE::GetTaskInterface();
	if (!a_event || !tasks) {
		return RE::BSEventNotifyControl::kContinue;
	}
	// Fires lit or put out by scripts are placed, enabled or disabled, so their 3D loading is the only signal.
	bool first = false;
	{
		auto&                  pending = GetPendingLoads();
		const std::scoped_lock lock{ pending.mutex };
		first = pending.loads.empty();
		pending.loads.emplace_back(a_event->formID, a_event->loaded);
	}
	if (first) {
		tasks->AddTask(CheckPendingLoads);
	}
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl WarmHands::GameEvents::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source)
{
	// Opening menus are checked too, so a pause restarts the stand-still timer.
	return CheckIf(a_event);
}

RE::BSEventNotifyControl WarmHands::GameEvents::ProcessEvent(const RE::BSAnimationGraphEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
{
	if (!a_event || !IsPlayer(a_event->holder)) {
		return RE::BSEventNotifyControl::kContinue;
	}
	const std::string_view tag{ a_event->tag.c_str() };
	const bool             relevant = std::ranges::any_of(kAnimationTags, [tag](std::string_view known) { return WarmHands::NamesMatch(tag, known); });
	if (relevant) {
		logger::debug("Animation event {}", tag);
	}
	return CheckIf(relevant);
}
