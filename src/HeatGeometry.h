#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace WarmHands
{
	struct Vec2
	{
		float x{ 0.0F };
		float y{ 0.0F };
	};

	struct Footprint
	{
		float minX{ 0.0F };
		float minY{ 0.0F };
		float maxX{ 0.0F };
		float maxY{ 0.0F };
	};

	[[nodiscard]] inline float FlameRadius(const Footprint& footprint)
	{
		return std::max(std::min({ -footprint.minX, footprint.maxX, -footprint.minY, footprint.maxY }), 0.0F);
	}

	[[nodiscard]] inline float Length(Vec2 v)
	{
		return std::hypot(v.x, v.y);
	}

	[[nodiscard]] inline Vec2 RotateToWorld(Vec2 local, float heading)
	{
		const float c = std::cos(heading);
		const float s = std::sin(heading);
		return { .x = (local.x * c) + (local.y * s), .y = (local.y * c) - (local.x * s) };
	}

	[[nodiscard]] inline Vec2 RotateToLocal(Vec2 world, float heading)
	{
		return RotateToWorld(world, -heading);
	}

	[[nodiscard]] inline Vec2 ToNearestEdge(Vec2 playerOffset, float objectHeading, const Footprint& footprint)
	{
		const Vec2 local = RotateToLocal(playerOffset, objectHeading);
		const Vec2 nearest{ .x = std::clamp(local.x, footprint.minX, footprint.maxX), .y = std::clamp(local.y, footprint.minY, footprint.maxY) };
		return RotateToWorld({ .x = nearest.x - local.x, .y = nearest.y - local.y }, objectHeading);
	}

	[[nodiscard]] inline bool IsWithinAngle(Vec2 direction, Vec2 toTarget, float halfAngle)
	{
		const float lengths = Length(direction) * Length(toTarget);
		if (lengths == 0.0F) {
			return true;
		}
		const float cosine = ((direction.x * toTarget.x) + (direction.y * toTarget.y)) / lengths;
		return cosine >= std::cos(halfAngle);
	}

	[[nodiscard]] inline bool IsFacing(float playerHeading, Vec2 toTarget, float halfAngle)
	{
		return IsWithinAngle(RotateToWorld({ .x = 0.0F, .y = 1.0F }, playerHeading), toTarget, halfAngle);
	}

	[[nodiscard]] inline Vec2 PointAtBearing(Vec2 centre, float radius, float bearing)
	{
		return { .x = centre.x + (radius * std::sin(bearing)), .y = centre.y + (radius * std::cos(bearing)) };
	}

	[[nodiscard]] inline float BearingOf(Vec2 direction)
	{
		return std::atan2(direction.x, direction.y);
	}

	[[nodiscard]] inline std::vector<Vec2> Arc(Vec2 centre, float radius, float fromBearing, float toBearing, int segments)
	{
		std::vector<Vec2> points;
		for (int i = 0; i <= segments; ++i) {
			const float t = static_cast<float>(i) / static_cast<float>(segments);
			points.push_back(PointAtBearing(centre, radius, fromBearing + ((toBearing - fromBearing) * t)));
		}
		return points;
	}

	[[nodiscard]] inline std::vector<Vec2> RoundedOutline(const Footprint& footprint, float reach, int cornerSegments)
	{
		constexpr float           kQuarter = std::numbers::pi_v<float> / 2.0F;
		const std::array<Vec2, 4> corners{ {
			{ .x = footprint.maxX, .y = footprint.maxY },
			{ .x = footprint.maxX, .y = footprint.minY },
			{ .x = footprint.minX, .y = footprint.minY },
			{ .x = footprint.minX, .y = footprint.maxY },
		} };
		std::vector<Vec2>         points;
		float                     bearing = 0.0F;
		for (const Vec2 corner : corners) {
			const auto arc = Arc(corner, reach, bearing, bearing + kQuarter, cornerSegments);
			points.insert(points.end(), arc.begin(), arc.end());
			bearing += kQuarter;
		}
		return points;
	}

	[[nodiscard]] inline bool IsFacingFootprint(float playerHeading, Vec2 playerOffset, float objectHeading, const Footprint& footprint, float halfAngle)
	{
		const Vec2 local = RotateToLocal(playerOffset, objectHeading);
		const bool inside = local.x >= footprint.minX && local.x <= footprint.maxX && local.y >= footprint.minY && local.y <= footprint.maxY;
		if (inside) {
			return true;
		}
		constexpr float           kPi = std::numbers::pi_v<float>;
		const std::array<Vec2, 4> corners{ {
			{ .x = footprint.minX, .y = footprint.minY },
			{ .x = footprint.minX, .y = footprint.maxY },
			{ .x = footprint.maxX, .y = footprint.minY },
			{ .x = footprint.maxX, .y = footprint.maxY },
		} };
		// Seen from outside, every corner lies within half a turn of the centre, so bearings relative to it never wrap.
		const Vec2  toCentre{ .x = ((footprint.minX + footprint.maxX) / 2.0F) - local.x, .y = ((footprint.minY + footprint.maxY) / 2.0F) - local.y };
		const float reference = BearingOf(RotateToWorld(toCentre, objectHeading));
		float       lowest = kPi;
		float       highest = -kPi;
		for (const Vec2 corner : corners) {
			const Vec2  world = RotateToWorld({ .x = corner.x - local.x, .y = corner.y - local.y }, objectHeading);
			const float relative = std::remainder(BearingOf(world) - reference, 2.0F * kPi);
			lowest = std::min(lowest, relative);
			highest = std::max(highest, relative);
		}
		const float heading = std::remainder(playerHeading - reference, 2.0F * kPi);
		return heading >= lowest - halfAngle && heading <= highest + halfAngle;
	}

	[[nodiscard]] inline float BoxExtentAlong(Vec2 direction, float heading, Vec2 centre, float halfWidth, float halfDepth)
	{
		const float length = Length(direction);
		if (length == 0.0F) {
			return centre.y + halfDepth;
		}
		const Vec2  local = RotateToLocal({ .x = direction.x / length, .y = direction.y / length }, heading);
		const float offset = (centre.x * local.x) + (centre.y * local.y);
		return offset + (std::abs(local.x) * halfWidth) + (std::abs(local.y) * halfDepth);
	}
}
