#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include "KeyNames.h"

namespace WarmHands
{
	inline constexpr int kDefaultIdleDelay = 3;

	struct Settings
	{
		bool        enabled{ true };
		bool        enableFireMagicWarmth{ true };
		bool        requireActiveCast{ false };
		bool        requireBothHands{ false };
		bool        warmFromActiveFireEffects{ true };
		bool        enableAnimation{ true };
		bool        autoWarmHands{ true };
		int         idleDelay{ kDefaultIdleDelay };
		bool        holdToWarmHands{ true };
		std::string holdKeyboardKey{ "R" };
		std::string holdGamepadButton{ "X" };
		float       holdDuration{ 0.5F };
		float       warmHandsRadius{ 100.0F };
		bool        showActivationRange{ false };
		bool        warmHandsAtFires{ true };
		bool        warmHandsAtForges{ true };
		bool        warmHandsAtOtherHeat{ true };
		int         logLevel{ 2 };

		bool operator==(const Settings&) const = default;
	};

	[[nodiscard]] inline Settings ActiveSettings(Settings settings)
	{
		if (!settings.enabled) {
			settings.enableFireMagicWarmth = false;
			settings.showActivationRange = false;
		}
		if (!settings.enabled || !settings.enableAnimation) {
			settings.autoWarmHands = false;
			settings.holdToWarmHands = false;
		}
		return settings;
	}

	enum class HeatKind
	{
		kFire,
		kForge,
		kOther,
	};

	[[nodiscard]] constexpr bool WarmsHandsAt(HeatKind kind, const Settings& settings) noexcept
	{
		switch (kind) {
		case HeatKind::kFire:
			return settings.warmHandsAtFires;
		case HeatKind::kForge:
			return settings.warmHandsAtForges;
		case HeatKind::kOther:
			return settings.warmHandsAtOtherHeat;
		}
		return false;
	}

	inline constexpr float kMinHoldDuration = 0.2F;
	inline constexpr float kMaxHoldDuration = 5.0F;
	inline constexpr float kMinWarmHandsRadius = 35.0F;
	inline constexpr float kMaxWarmHandsRadius = 300.0F;
	inline constexpr int   kMinIdleDelay = 1;
	inline constexpr int   kMaxIdleDelay = 60;
	inline constexpr int   kMinLogLevel = 0;
	inline constexpr int   kMaxLogLevel = 6;
	inline constexpr float kUnitsPerMetre = 70.0F;

	[[nodiscard]] constexpr float UnitsToMetres(float units) noexcept
	{
		return units / kUnitsPerMetre;
	}

	[[nodiscard]] constexpr float MetresToUnits(float metres) noexcept
	{
		return metres * kUnitsPerMetre;
	}

	[[nodiscard]] inline float ClampOrDefault(float value, float defaultVal, float minVal, float maxVal)
	{
		if (!std::isfinite(value) || value < minVal) {
			return defaultVal;
		}
		if (value > maxVal) {
			return maxVal;
		}
		return value;
	}

	[[nodiscard]] constexpr int ClampOrDefault(int value, int defaultVal, int minVal, int maxVal) noexcept
	{
		if (value < minVal) {
			return defaultVal;
		}
		return std::min(value, maxVal);
	}

	[[nodiscard]] inline Settings Sanitize(Settings settings)
	{
		const Settings defaults{};
		settings.holdDuration = ClampOrDefault(settings.holdDuration, defaults.holdDuration, kMinHoldDuration, kMaxHoldDuration);
		settings.warmHandsRadius = ClampOrDefault(settings.warmHandsRadius, defaults.warmHandsRadius, kMinWarmHandsRadius, kMaxWarmHandsRadius);
		if (!KeyboardKeyCode(settings.holdKeyboardKey)) {
			settings.holdKeyboardKey = defaults.holdKeyboardKey;
		}
		if (!GamepadButtonCode(settings.holdGamepadButton)) {
			settings.holdGamepadButton = defaults.holdGamepadButton;
		}
		settings.idleDelay = ClampOrDefault(settings.idleDelay, defaults.idleDelay, kMinIdleDelay, kMaxIdleDelay);
		if (settings.logLevel < kMinLogLevel || settings.logLevel > kMaxLogLevel) {
			settings.logLevel = defaults.logLevel;
		}
		return settings;
	}

	inline void ResetMagicSettings(Settings& settings)
	{
		const Settings defaults{};
		settings.enableFireMagicWarmth = defaults.enableFireMagicWarmth;
		settings.requireActiveCast = defaults.requireActiveCast;
		settings.requireBothHands = defaults.requireBothHands;
		settings.warmFromActiveFireEffects = defaults.warmFromActiveFireEffects;
	}

	inline void ResetAnimationSettings(Settings& settings)
	{
		const Settings defaults{};
		settings.enableAnimation = defaults.enableAnimation;
		settings.autoWarmHands = defaults.autoWarmHands;
		settings.idleDelay = defaults.idleDelay;
		settings.holdToWarmHands = defaults.holdToWarmHands;
		settings.holdKeyboardKey = defaults.holdKeyboardKey;
		settings.holdGamepadButton = defaults.holdGamepadButton;
		settings.holdDuration = defaults.holdDuration;
		settings.warmHandsRadius = defaults.warmHandsRadius;
		settings.warmHandsAtFires = defaults.warmHandsAtFires;
		settings.warmHandsAtForges = defaults.warmHandsAtForges;
		settings.warmHandsAtOtherHeat = defaults.warmHandsAtOtherHeat;
	}
}
