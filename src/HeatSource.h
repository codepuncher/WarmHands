#pragma once

#include "HeatGeometry.h"
#include "Settings.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace RE
{
	class PlayerCharacter;
}

namespace WarmHands::HeatSource
{
	[[nodiscard]] bool Initialize();

	void Update(RE::PlayerCharacter& player, bool active);

	struct NearHeat
	{
		bool any{ false };
		bool autoEnabled{ false };
	};

	[[nodiscard]] NearHeat FindHeat(RE::PlayerCharacter& player, const Settings& settings, bool idlePlaying);

	struct Outline
	{
		std::vector<Vec2>   points;
		float               height{ 0.0F };
		std::string         label;
		Vec2                labelAt;
		Vec2                from;
		std::optional<Vec2> hit;
		bool                inReach{ false };
		bool                facing{ false };
	};

	[[nodiscard]] std::shared_ptr<const std::vector<Outline>> Outlines();

	void RemoveLeftovers(RE::PlayerCharacter& player);
}
