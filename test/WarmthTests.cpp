#include <catch2/catch_test_macros.hpp>

#include "WarmthSource.h"

using WarmHands::IsWarmedByFireMagic;
using WarmHands::PlayerMagicState;
using WarmHands::WarmthRules;

namespace
{
	constexpr WarmthRules kDefaultRules{ .requireActiveCast = false, .warmFromActiveFireEffects = true };
	constexpr WarmthRules kCastOnlyRules{ .requireActiveCast = true, .warmFromActiveFireEffects = true };
}

TEST_CASE("Equipped fire spell warms only while drawn", "[warmth]")
{
	PlayerMagicState state{};
	state.left.fireSpellEquipped = true;

	state.handsDrawn = true;
	CHECK(IsWarmedByFireMagic(state, kDefaultRules));

	state.handsDrawn = false;
	CHECK_FALSE(IsWarmedByFireMagic(state, kDefaultRules));
}

TEST_CASE("Either hand can hold the fire spell", "[warmth]")
{
	PlayerMagicState state{};
	state.handsDrawn = true;
	state.right.fireSpellEquipped = true;

	CHECK(IsWarmedByFireMagic(state, kDefaultRules));
}

TEST_CASE("Drawn hands without a fire spell do not warm", "[warmth]")
{
	PlayerMagicState state{};
	state.handsDrawn = true;

	CHECK_FALSE(IsWarmedByFireMagic(state, kDefaultRules));
}

TEST_CASE("Cast-only rule needs the spell to be cast", "[warmth]")
{
	PlayerMagicState state{};
	state.handsDrawn = true;
	state.left.fireSpellEquipped = true;

	CHECK_FALSE(IsWarmedByFireMagic(state, kCastOnlyRules));

	state.left.casting = true;
	CHECK(IsWarmedByFireMagic(state, kCastOnlyRules));
}

TEST_CASE("Casting a non-fire spell does not warm", "[warmth]")
{
	PlayerMagicState state{};
	state.handsDrawn = true;
	state.left.casting = true;

	CHECK_FALSE(IsWarmedByFireMagic(state, kCastOnlyRules));
}

TEST_CASE("Self-cast fire effect warms while sheathed when enabled", "[warmth]")
{
	PlayerMagicState state{};
	state.selfFireEffectActive = true;

	CHECK(IsWarmedByFireMagic(state, kDefaultRules));
	CHECK(IsWarmedByFireMagic(state, kCastOnlyRules));

	constexpr WarmthRules noEffects{ .requireActiveCast = false, .warmFromActiveFireEffects = false };
	CHECK_FALSE(IsWarmedByFireMagic(state, noEffects));
}

TEST_CASE("Both hands rule needs a fire spell drawn in each hand", "[warmth]")
{
	constexpr WarmthRules bothHands{ .requireActiveCast = false, .requireBothHands = true, .warmFromActiveFireEffects = true };
	PlayerMagicState      state{};
	state.handsDrawn = true;
	state.left.fireSpellEquipped = true;
	CHECK_FALSE(IsWarmedByFireMagic(state, bothHands));

	state.right.fireSpellEquipped = true;
	CHECK(IsWarmedByFireMagic(state, bothHands));
}

TEST_CASE("Both hands rule with casting needs both hands casting", "[warmth]")
{
	constexpr WarmthRules bothCasting{ .requireActiveCast = true, .requireBothHands = true, .warmFromActiveFireEffects = true };
	PlayerMagicState      state{};
	state.left = { .fireSpellEquipped = true, .casting = true };
	state.right = { .fireSpellEquipped = true, .casting = false };
	CHECK_FALSE(IsWarmedByFireMagic(state, bothCasting));

	state.right.casting = true;
	CHECK(IsWarmedByFireMagic(state, bothCasting));
}

TEST_CASE("Both hands rule still warms from an active fire effect", "[warmth]")
{
	constexpr WarmthRules bothHands{ .requireActiveCast = false, .requireBothHands = true, .warmFromActiveFireEffects = true };
	PlayerMagicState      state{};
	state.selfFireEffectActive = true;

	CHECK(IsWarmedByFireMagic(state, bothHands));
}

TEST_CASE("Both hands rule counts a two-handed fire spell as both hands", "[warmth]")
{
	constexpr WarmthRules bothHands{ .requireActiveCast = false, .requireBothHands = true, .warmFromActiveFireEffects = true };
	PlayerMagicState      state{};
	state.handsDrawn = true;
	state.right = { .fireSpellEquipped = true, .casting = false, .twoHanded = true };

	CHECK(IsWarmedByFireMagic(state, bothHands));
}

TEST_CASE("Both hands rule counts a dual cast as both hands casting", "[warmth]")
{
	constexpr WarmthRules bothCasting{ .requireActiveCast = true, .requireBothHands = true, .warmFromActiveFireEffects = true };
	PlayerMagicState      state{};
	state.left = { .fireSpellEquipped = true, .casting = true };
	state.right = { .fireSpellEquipped = true, .casting = false };
	state.dualCasting = true;

	CHECK(IsWarmedByFireMagic(state, bothCasting));
}
