#include "PCH.h"

#include "FireMagic.h"

namespace
{
	constexpr RE::FormID kMagicDamageFire = 0x0001CEAD;

	const RE::BGSKeyword* FireKeyword()
	{
		static const auto* keyword = RE::TESForm::LookupByID<RE::BGSKeyword>(kMagicDamageFire);
		return keyword;
	}

	bool HasFireKeyword(const RE::EffectSetting* effect)
	{
		const auto* keyword = FireKeyword();
		return effect && keyword && effect->HasKeyword(keyword);
	}

	bool AnyEffect(const RE::MagicItem* item, bool (*predicate)(const RE::EffectSetting*))
	{
		if (!item) {
			return false;
		}
		return std::ranges::any_of(item->effects, [predicate](const RE::Effect* effect) {
			return effect && predicate(effect->baseEffect);
		});
	}
}

bool WarmHands::FireMagic::CountsAsFire(const RE::EffectSetting* effect)
{
	if (!effect) {
		return false;
	}
	if (HasFireKeyword(effect)) {
		return true;
	}
	if (effect->GetArchetype() != RE::EffectSetting::Archetype::kCloak) {
		return false;
	}
	const auto* associated = effect->data.associatedForm;
	return associated && AnyEffect(associated->As<RE::MagicItem>(), HasFireKeyword);
}

bool WarmHands::FireMagic::IsFireSpell(const RE::MagicItem* item)
{
	return AnyEffect(item, CountsAsFire);
}
