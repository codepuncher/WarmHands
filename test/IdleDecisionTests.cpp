#include <catch2/catch_test_macros.hpp>

#include "IdleDecision.h"

using WarmHands::DecideIdle;
using WarmHands::IdleAction;
using WarmHands::IdleInputs;

namespace
{
	IdleInputs ReadyToWarm()
	{
		IdleInputs in{};
		in.nearHeat = true;
		in.stillSeconds = in.idleDelaySeconds;
		return in;
	}
}

TEST_CASE("DecideIdle starts when standing still near heat with free hands", "[idle]")
{
	CHECK(DecideIdle(ReadyToWarm(), false, true) == IdleAction::kStart);
	CHECK(DecideIdle(ReadyToWarm(), true, true) == IdleAction::kNone);
}

TEST_CASE("DecideIdle waits for the settle time", "[idle]")
{
	auto in = ReadyToWarm();
	in.stillSeconds = in.idleDelaySeconds - 1.0F;

	CHECK(DecideIdle(in, false, true) == IdleAction::kNone);
}

TEST_CASE("DecideIdle waits for a custom delay", "[idle]")
{
	auto in = ReadyToWarm();
	in.idleDelaySeconds = 10.0F;
	in.stillSeconds = 9.0F;
	CHECK(DecideIdle(in, false, true) == IdleAction::kNone);

	in.stillSeconds = 10.0F;
	CHECK(DecideIdle(in, false, true) == IdleAction::kStart);
}

TEST_CASE("DecideIdle does not start away from heat or with busy hands", "[idle]")
{
	auto noHeat = ReadyToWarm();
	noHeat.nearHeat = false;
	CHECK(DecideIdle(noHeat, false, true) == IdleAction::kNone);

	auto torch = ReadyToWarm();
	torch.holdingTorch = true;
	CHECK(DecideIdle(torch, false, true) == IdleAction::kNone);

	auto combat = ReadyToWarm();
	combat.inCombat = true;
	CHECK(DecideIdle(combat, false, true) == IdleAction::kNone);

	auto mounted = ReadyToWarm();
	mounted.mounted = true;
	CHECK(DecideIdle(mounted, false, true) == IdleAction::kNone);
}

TEST_CASE("DecideIdle does not start while sitting, swimming or in dialogue", "[idle]")
{
	auto in = ReadyToWarm();
	in.busy = true;

	CHECK(DecideIdle(in, false, true) == IdleAction::kNone);
}

TEST_CASE("DecideIdle forgets the idle when furniture or dialogue takes over", "[idle]")
{
	auto in = ReadyToWarm();
	in.busy = true;

	CHECK(DecideIdle(in, true, true) == IdleAction::kForget);
}

TEST_CASE("DecideIdle stops the idle when heat is lost while standing", "[idle]")
{
	auto in = ReadyToWarm();
	in.nearHeat = false;

	CHECK(DecideIdle(in, true, true) == IdleAction::kStop);
}

TEST_CASE("DecideIdle stops the idle when fire magic starts warming", "[idle]")
{
	auto in = ReadyToWarm();
	in.warmedByFireMagic = true;

	CHECK(DecideIdle(in, false, true) == IdleAction::kNone);
	CHECK(DecideIdle(in, true, true) == IdleAction::kStop);
}

TEST_CASE("DecideIdle forgets the idle when the player already broke it", "[idle]")
{
	auto moved = ReadyToWarm();
	moved.stillSeconds = 0.0F;
	CHECK(DecideIdle(moved, true, true) == IdleAction::kForget);

	auto drawn = ReadyToWarm();
	drawn.handsDrawn = true;
	CHECK(DecideIdle(drawn, true, true) == IdleAction::kForget);
}

TEST_CASE("DecideIdle does not auto start when auto is off", "[idle]")
{
	CHECK(DecideIdle(ReadyToWarm(), false, false) == IdleAction::kNone);

	auto in = ReadyToWarm();
	in.nearHeat = false;
	CHECK(DecideIdle(in, true, false) == IdleAction::kStop);
}

TEST_CASE("CanStartIdle skips the settle time but needs the player standing", "[idle]")
{
	using WarmHands::CanStartIdle;

	auto in = ReadyToWarm();
	in.stillSeconds = 0.5F;
	CHECK(CanStartIdle(in));

	in.stillSeconds = 0.0F;
	CHECK_FALSE(CanStartIdle(in));
}

TEST_CASE("CanStartIdle needs heat and free hands", "[idle]")
{
	using WarmHands::CanStartIdle;

	auto noHeat = ReadyToWarm();
	noHeat.nearHeat = false;
	CHECK_FALSE(CanStartIdle(noHeat));

	auto drawn = ReadyToWarm();
	drawn.handsDrawn = true;
	CHECK_FALSE(CanStartIdle(drawn));

	auto busy = ReadyToWarm();
	busy.busy = true;
	CHECK_FALSE(CanStartIdle(busy));
}

TEST_CASE("DecideIdle keeps a hold-started idle before the settle time", "[idle]")
{
	auto in = ReadyToWarm();
	in.stillSeconds = 1.0F;

	CHECK(DecideIdle(in, true, true) == IdleAction::kNone);
	CHECK(DecideIdle(in, false, true) == IdleAction::kNone);
}

TEST_CASE("KeepDismissed holds while the player stays at the heat", "[idle]")
{
	using WarmHands::KeepDismissed;

	CHECK(KeepDismissed(true, ReadyToWarm()));
	CHECK_FALSE(KeepDismissed(false, ReadyToWarm()));
}

TEST_CASE("KeepDismissed clears once the player moves or leaves the heat", "[idle]")
{
	using WarmHands::KeepDismissed;

	auto moved = ReadyToWarm();
	moved.stillSeconds = 0.0F;
	CHECK_FALSE(KeepDismissed(true, moved));

	auto noHeat = ReadyToWarm();
	noHeat.nearHeat = false;
	CHECK_FALSE(KeepDismissed(true, noHeat));
}

TEST_CASE("DecideIdle does not auto start at a heat source with auto turned off", "[idle]")
{
	auto in = ReadyToWarm();
	in.autoHeat = false;

	CHECK(DecideIdle(in, false, true) == IdleAction::kNone);
}

TEST_CASE("A manual idle keeps playing at a heat source with auto turned off", "[idle]")
{
	using WarmHands::CanStartIdle;

	auto in = ReadyToWarm();
	in.autoHeat = false;

	CHECK(CanStartIdle(in));
	CHECK(DecideIdle(in, true, true) == IdleAction::kNone);
}
