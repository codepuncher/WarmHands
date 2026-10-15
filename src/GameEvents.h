#pragma once

namespace WarmHands
{
	class GameEvents final :
	    public RE::BSTEventSink<RE::TESEquipEvent>,
	    public RE::BSTEventSink<RE::TESActiveEffectApplyRemoveEvent>,
	    public RE::BSTEventSink<RE::TESCombatEvent>,
	    public RE::BSTEventSink<RE::TESFurnitureEvent>,
	    public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
	    public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
	    public RE::BSTEventSink<RE::BSAnimationGraphEvent>
	{
	public:
		static GameEvents* GetSingleton();

		GameEvents(const GameEvents&) = delete;
		GameEvents(GameEvents&&) = delete;
		GameEvents& operator=(const GameEvents&) = delete;
		GameEvents& operator=(GameEvents&&) = delete;
		~GameEvents() override = default;

		void Register();

		void AttachToPlayer();

		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>* a_source) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::TESActiveEffectApplyRemoveEvent* a_event, RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>* a_source) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>* a_source) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* a_event, RE::BSTEventSource<RE::TESFurnitureEvent>* a_source) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_source) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;
		RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source) override;

	private:
		GameEvents() = default;
	};
}
