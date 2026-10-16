#include <catch2/catch_test_macros.hpp>

#include "HoldTracker.h"

using WarmHands::HoldOutcome;
using WarmHands::HoldTracker;

namespace
{
	constexpr float kDuration = 0.5F;
}

TEST_CASE("HoldTracker leaves an unarmed press to the game", "[hold]")
{
	HoldTracker tracker;
	CHECK(tracker.OnDown(false, false) == HoldOutcome::kPass);
	CHECK(tracker.OnHeld(1.0F, kDuration) == HoldOutcome::kPass);
	CHECK(tracker.OnUp(1.0F, kDuration) == HoldOutcome::kPass);
}

TEST_CASE("HoldTracker replays a short press", "[hold]")
{
	HoldTracker tracker;
	CHECK(tracker.OnDown(true, false) == HoldOutcome::kBlock);
	CHECK(tracker.OnHeld(0.2F, kDuration) == HoldOutcome::kBlock);
	CHECK(tracker.OnUp(0.3F, kDuration) == HoldOutcome::kReplayShortPress);
}

TEST_CASE("HoldTracker starts the idle once on a long press", "[hold]")
{
	HoldTracker tracker;
	CHECK(tracker.OnDown(true, false) == HoldOutcome::kBlock);
	CHECK(tracker.OnHeld(0.5F, kDuration) == HoldOutcome::kStartIdle);
	CHECK(tracker.OnHeld(0.9F, kDuration) == HoldOutcome::kBlock);
	CHECK(tracker.OnUp(1.0F, kDuration) == HoldOutcome::kBlock);
}

TEST_CASE("HoldTracker starts the idle on a long release with no held events", "[hold]")
{
	HoldTracker tracker;
	CHECK(tracker.OnDown(true, false) == HoldOutcome::kBlock);
	CHECK(tracker.OnUp(0.6F, kDuration) == HoldOutcome::kStartIdle);
}

TEST_CASE("HoldTracker ends a playing idle on a short press instead of replaying it", "[hold]")
{
	HoldTracker tracker;
	CHECK(tracker.OnDown(true, true) == HoldOutcome::kBlock);
	CHECK(tracker.OnUp(0.2F, kDuration) == HoldOutcome::kStopIdle);
}

TEST_CASE("HoldTracker keeps a playing idle on a long press", "[hold]")
{
	HoldTracker tracker;
	CHECK(tracker.OnDown(true, true) == HoldOutcome::kBlock);
	CHECK(tracker.OnHeld(0.6F, kDuration) == HoldOutcome::kStartIdle);
	CHECK(tracker.OnUp(0.8F, kDuration) == HoldOutcome::kBlock);
}
