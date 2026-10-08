#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

#include "HeatGeometry.h"

using Catch::Approx;
using WarmHands::Footprint;
using WarmHands::IsFacing;
using WarmHands::Length;
using WarmHands::ToNearestEdge;
using WarmHands::Vec2;

namespace
{
	constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;
	constexpr float kHalfTurn = std::numbers::pi_v<float>;
	constexpr float kFortyFiveDegrees = std::numbers::pi_v<float> / 4.0F;
}

TEST_CASE("ToNearestEdge measures to the edge of an unrotated footprint", "[geometry]")
{
	const Footprint box{ .minX = -50.0F, .minY = -50.0F, .maxX = 50.0F, .maxY = 50.0F };

	const auto toEdge = ToNearestEdge({ .x = 0.0F, .y = -150.0F }, 0.0F, box);

	CHECK(toEdge.x == Approx(0.0F).margin(0.001));
	CHECK(toEdge.y == Approx(100.0F));
	CHECK(Length(toEdge) == Approx(100.0F));
}

TEST_CASE("ToNearestEdge follows the footprint's rotation", "[geometry]")
{
	const Footprint longBox{ .minX = -100.0F, .minY = -20.0F, .maxX = 100.0F, .maxY = 20.0F };

	const auto toEdge = ToNearestEdge({ .x = 0.0F, .y = -150.0F }, kQuarterTurn, longBox);

	CHECK(toEdge.x == Approx(0.0F).margin(0.001));
	CHECK(toEdge.y == Approx(50.0F));
}

TEST_CASE("ToNearestEdge is zero inside the footprint", "[geometry]")
{
	const Footprint box{ .minX = -50.0F, .minY = -50.0F, .maxX = 50.0F, .maxY = 50.0F };

	CHECK(Length(ToNearestEdge({ .x = 10.0F, .y = -20.0F }, 0.0F, box)) == Approx(0.0F).margin(0.001));
}

TEST_CASE("IsFacing accepts targets inside the cone ahead", "[geometry]")
{
	CHECK(IsFacing(0.0F, { .x = 0.0F, .y = 100.0F }, kFortyFiveDegrees));
	CHECK(IsFacing(0.0F, { .x = 90.0F, .y = 100.0F }, kFortyFiveDegrees));
	CHECK(IsFacing(kHalfTurn, { .x = 0.0F, .y = -100.0F }, kFortyFiveDegrees));
}

TEST_CASE("IsFacing rejects targets beside or behind", "[geometry]")
{
	CHECK_FALSE(IsFacing(0.0F, { .x = 0.0F, .y = -100.0F }, kFortyFiveDegrees));
	CHECK_FALSE(IsFacing(0.0F, { .x = 100.0F, .y = 0.0F }, kFortyFiveDegrees));
	CHECK_FALSE(IsFacing(kQuarterTurn, { .x = -100.0F, .y = 0.0F }, kFortyFiveDegrees));
}

TEST_CASE("IsFacing accepts a target the player stands inside", "[geometry]")
{
	CHECK(IsFacing(kHalfTurn, { .x = 0.0F, .y = 0.0F }, kFortyFiveDegrees));
}

TEST_CASE("IsFacingFootprint accepts any heading from inside the footprint", "[geometry]")
{
	const Footprint box{ .minX = -100.0F, .minY = -100.0F, .maxX = 100.0F, .maxY = 100.0F };

	CHECK(WarmHands::IsFacingFootprint(kHalfTurn, { .x = 0.0F, .y = 50.0F }, 0.0F, box, kFortyFiveDegrees));
}

TEST_CASE("IsFacingFootprint counts any part of the footprint inside the cone", "[geometry]")
{
	using WarmHands::IsFacingFootprint;

	const Footprint box{ .minX = -100.0F, .minY = -100.0F, .maxX = 100.0F, .maxY = 100.0F };
	const Vec2      offCorner{ .x = 150.0F, .y = -150.0F };
	constexpr float kTwentyDegrees = std::numbers::pi_v<float> / 9.0F;

	CHECK(IsFacingFootprint(kTwentyDegrees, offCorner, 0.0F, box, kFortyFiveDegrees));
	CHECK_FALSE(IsFacingFootprint(kHalfTurn, offCorner, 0.0F, box, kFortyFiveDegrees));
	CHECK_FALSE(IsFacingFootprint(kHalfTurn, { .x = 0.0F, .y = -150.0F }, 0.0F, box, kFortyFiveDegrees));
	const Footprint rotatedBox{ .minX = -100.0F, .minY = -100.0F, .maxX = 300.0F, .maxY = 100.0F };
	const Vec2      south{ .x = 0.0F, .y = -400.0F };
	CHECK(IsFacingFootprint(0.0F, south, kQuarterTurn, rotatedBox, kFortyFiveDegrees));
	CHECK_FALSE(IsFacingFootprint(kHalfTurn, south, kQuarterTurn, rotatedBox, kFortyFiveDegrees));
}

TEST_CASE("IsFacingFootprint handles a footprint that reaches behind the player", "[geometry]")
{
	using WarmHands::IsFacingFootprint;

	const Footprint longBox{ .minX = -300.0F, .minY = 10.0F, .maxX = 300.0F, .maxY = 40.0F };
	constexpr float kDegrees = std::numbers::pi_v<float> / 180.0F;
	const Vec2      origin{};

	CHECK(IsFacingFootprint(130.0F * kDegrees, origin, 0.0F, longBox, kFortyFiveDegrees));
	CHECK_FALSE(IsFacingFootprint(kHalfTurn, origin, 0.0F, longBox, kFortyFiveDegrees));
}

TEST_CASE("IsWithinAngle compares two directions", "[geometry]")
{
	using WarmHands::IsWithinAngle;

	CHECK(IsWithinAngle({ .x = 1.0F, .y = 0.0F }, { .x = 100.0F, .y = 50.0F }, kFortyFiveDegrees));
	CHECK_FALSE(IsWithinAngle({ .x = 1.0F, .y = 0.0F }, { .x = 0.0F, .y = 100.0F }, kFortyFiveDegrees));
	CHECK_FALSE(IsWithinAngle({ .x = 1.0F, .y = 0.0F }, { .x = -100.0F, .y = 0.0F }, kFortyFiveDegrees));
}

TEST_CASE("FlameRadius keeps the side nearest the origin", "[geometry]")
{
	using WarmHands::FlameRadius;

	CHECK(FlameRadius({ .minX = -62.0F, .minY = -113.0F, .maxX = 94.0F, .maxY = 62.0F }) == Approx(62.0F));
	CHECK(FlameRadius({ .minX = -125.0F, .minY = -82.0F, .maxX = 134.0F, .maxY = 66.0F }) == Approx(66.0F));
	CHECK(FlameRadius({ .minX = 10.0F, .minY = -20.0F, .maxX = 50.0F, .maxY = 40.0F }) == Approx(0.0F));
}

TEST_CASE("PointAtBearing follows Skyrim headings", "[geometry]")
{
	const auto north = WarmHands::PointAtBearing({ .x = 10.0F, .y = 10.0F }, 100.0F, 0.0F);
	CHECK(north.x == Approx(10.0F));
	CHECK(north.y == Approx(110.0F));

	const auto east = WarmHands::PointAtBearing({ .x = 10.0F, .y = 10.0F }, 100.0F, kQuarterTurn);
	CHECK(east.x == Approx(110.0F));
	CHECK(east.y == Approx(10.0F).margin(0.01));

	CHECK(WarmHands::BearingOf({ .x = 1.0F, .y = 0.0F }) == Approx(kQuarterTurn));
}

TEST_CASE("RoundedOutline keeps reach away from every side", "[geometry]")
{
	const Footprint box{ .minX = -50.0F, .minY = -20.0F, .maxX = 50.0F, .maxY = 20.0F };
	const auto      outline = WarmHands::RoundedOutline(box, 30.0F, 4);

	REQUIRE(outline.size() == 20);
	for (const Vec2 point : outline) {
		CHECK(Length(ToNearestEdge(point, 0.0F, box)) == Approx(30.0F).margin(0.01));
	}
}

TEST_CASE("BoxExtentAlong reaches further ahead than to the side", "[geometry]")
{
	using WarmHands::BoxExtentAlong;

	const Vec2 centred{};
	CHECK(BoxExtentAlong({ .x = 0.0F, .y = 50.0F }, 0.0F, centred, 21.5F, 29.5F) == Approx(29.5F));
	CHECK(BoxExtentAlong({ .x = 50.0F, .y = 0.0F }, 0.0F, centred, 21.5F, 29.5F) == Approx(21.5F));
	CHECK(BoxExtentAlong({ .x = 50.0F, .y = 0.0F }, kQuarterTurn, centred, 21.5F, 29.5F) == Approx(29.5F));

	const Vec2 ahead{ .x = 0.0F, .y = 30.0F };
	CHECK(BoxExtentAlong({ .x = 0.0F, .y = 50.0F }, 0.0F, ahead, 21.5F, 29.5F) == Approx(59.5F));
	CHECK(BoxExtentAlong({ .x = 50.0F, .y = 0.0F }, 0.0F, ahead, 21.5F, 29.5F) == Approx(21.5F));
	CHECK(BoxExtentAlong({ .x = 0.0F, .y = -50.0F }, 0.0F, ahead, 21.5F, 29.5F) == Approx(-0.5F));
}
