#include "PCH.h"

#include "FireMagic.h"
#include "PlayerState.h"

namespace
{
	bool IsCasting(RE::PlayerCharacter& player, RE::MagicSystem::CastingSource source)
	{
		const auto* caster = player.GetMagicCaster(source);
		if (!caster) {
			return false;
		}
		using State = RE::MagicCaster::State;
		const auto state = caster->state.get();
		return state == State::kCharging || state == State::kCasting;
	}

	WarmHands::HandState ReadHand(RE::PlayerCharacter& player, bool leftHand)
	{
		const auto* equipped = player.GetEquippedObject(leftHand);
		const auto* item = equipped ? equipped->As<RE::MagicItem>() : nullptr;
		if (!item || !WarmHands::FireMagic::IsFireSpell(item)) {
			return {};
		}
		using Source = RE::MagicSystem::CastingSource;
		const auto own = leftHand ? Source::kLeftHand : Source::kRightHand;
		const auto other = leftHand ? Source::kRightHand : Source::kLeftHand;
		const bool twoHanded = item->IsTwoHanded();
		const bool casting = IsCasting(player, own) || (twoHanded && IsCasting(player, other));
		return { .fireSpellEquipped = true, .casting = casting, .twoHanded = twoHanded };
	}

	bool HasSelfCastFireEffect(RE::PlayerCharacter& player)
	{
		using Flag = RE::ActiveEffect::Flag;
		using Result = RE::BSContainer::ForEachResult;

		bool found = false;
		player.AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* effect) {
			if (!effect || effect->flags.any(Flag::kInactive, Flag::kDispelled)) {
				return Result::kContinue;
			}
			if (effect->GetCasterActor().get() != &player) {
				return Result::kContinue;
			}
			found = WarmHands::FireMagic::CountsAsFire(effect->GetBaseObject());
			return found ? Result::kStop : Result::kContinue;
		});
		return found;
	}
}

WarmHands::PlayerMagicState WarmHands::PlayerState::ReadMagicState(RE::PlayerCharacter& player)
{
	return {
		.left = ReadHand(player, true),
		.right = ReadHand(player, false),
		.handsDrawn = player.AsActorState()->IsWeaponDrawn(),
		.dualCasting = player.IsDualCasting(),
		.selfFireEffectActive = HasSelfCastFireEffect(player),
	};
}

bool WarmHands::PlayerState::IsStandingStill(RE::PlayerCharacter& player)
{
	return !player.IsMoving();
}

WarmHands::IdleInputs WarmHands::PlayerState::ReadIdleInputs(RE::PlayerCharacter& player)
{
	const auto* leftHand = player.GetEquippedObject(true);
	const auto* actorState = player.AsActorState();
	auto*       ui = RE::UI::GetSingleton();
	const bool  inDialogue = ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
	const auto* controlMap = RE::ControlMap::GetSingleton();
	const bool  controlsLocked = controlMap && (!controlMap->IsMovementControlsEnabled() || !controlMap->IsFightingControlsEnabled());
	return {
		.inCombat = player.IsInCombat(),
		.sneaking = player.IsSneaking(),
		.mounted = player.IsOnMount(),
		.handsDrawn = actorState->IsWeaponDrawn(),
		.holdingTorch = leftHand && leftHand->Is(RE::FormType::Light),
		.busy = actorState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal || actorState->IsSwimming() || inDialogue || controlsLocked,
	};
}
