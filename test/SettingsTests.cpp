#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "Settings.h"

using WarmHands::Sanitize;
using WarmHands::Settings;

TEST_CASE("Sanitize keeps in-range values", "[settings]")
{
	Settings in{};
	in.holdDuration = 1.5F;
	in.holdKeyboardKey = "G";
	in.holdGamepadButton = "RB";
	in.logLevel = 0;

	const auto out = Sanitize(in);

	CHECK(out.holdDuration == 1.5F);
	CHECK(out.holdKeyboardKey == "G");
	CHECK(out.holdGamepadButton == "RB");
	CHECK(out.logLevel == 0);
}

TEST_CASE("Sanitize clamps a hold duration above the maximum", "[settings]")
{
	Settings in{};
	in.holdDuration = 9.0F;

	CHECK(Sanitize(in).holdDuration == WarmHands::kMaxHoldDuration);
}

TEST_CASE("Sanitize falls back to defaults for invalid values", "[settings]")
{
	const Settings defaults{};
	Settings       in{};
	in.holdDuration = std::numeric_limits<float>::quiet_NaN();
	in.holdKeyboardKey = "Banana";
	in.holdGamepadButton = "Z";
	in.logLevel = 7;

	const auto out = Sanitize(in);

	CHECK(out.holdDuration == defaults.holdDuration);
	CHECK(out.holdKeyboardKey == defaults.holdKeyboardKey);
	CHECK(out.holdGamepadButton == defaults.holdGamepadButton);
	CHECK(out.logLevel == defaults.logLevel);

	in.logLevel = -1;
	in.holdDuration = 0.1F;
	CHECK(Sanitize(in).logLevel == defaults.logLevel);
	CHECK(Sanitize(in).holdDuration == defaults.holdDuration);
}

TEST_CASE("Sanitize leaves default settings unchanged", "[settings]")
{
	CHECK(Sanitize(Settings{}) == Settings{});
}

TEST_CASE("Sanitize bounds the warm hands radius", "[settings]")
{
	const Settings defaults{};
	Settings       in{};

	in.warmHandsRadius = 150.0F;
	CHECK(Sanitize(in).warmHandsRadius == 150.0F);

	in.warmHandsRadius = 2000.0F;
	CHECK(Sanitize(in).warmHandsRadius == WarmHands::kMaxWarmHandsRadius);

	in.warmHandsRadius = 10.0F;
	CHECK(Sanitize(in).warmHandsRadius == defaults.warmHandsRadius);

	in.warmHandsRadius = 400.0F;
	CHECK(Sanitize(in).warmHandsRadius == WarmHands::kMaxWarmHandsRadius);
}

TEST_CASE("Sanitize bounds the idle delay", "[settings]")
{
	const Settings defaults{};
	Settings       in{};

	in.idleDelay = 10;
	CHECK(Sanitize(in).idleDelay == 10);

	in.idleDelay = 0;
	CHECK(Sanitize(in).idleDelay == defaults.idleDelay);

	in.idleDelay = 90;
	CHECK(Sanitize(in).idleDelay == WarmHands::kMaxIdleDelay);
}

TEST_CASE("WarmsHandsAt follows each heat source toggle", "[settings]")
{
	using WarmHands::HeatKind;
	using WarmHands::WarmsHandsAt;

	Settings settings{};
	CHECK(WarmsHandsAt(HeatKind::kFire, settings));
	CHECK(WarmsHandsAt(HeatKind::kForge, settings));
	CHECK(WarmsHandsAt(HeatKind::kOther, settings));

	settings.warmHandsAtForges = false;
	CHECK(WarmsHandsAt(HeatKind::kFire, settings));
	CHECK_FALSE(WarmsHandsAt(HeatKind::kForge, settings));

	settings.warmHandsAtFires = false;
	settings.warmHandsAtOtherHeat = false;
	CHECK_FALSE(WarmsHandsAt(HeatKind::kFire, settings));
	CHECK_FALSE(WarmsHandsAt(HeatKind::kOther, settings));
}

TEST_CASE("ResetMagicSettings restores only the magic page", "[settings]")
{
	const Settings defaults{};
	Settings       settings{};
	settings.enableFireMagicWarmth = false;
	settings.requireActiveCast = true;
	settings.requireBothHands = true;
	settings.warmFromActiveFireEffects = false;
	settings.logLevel = 0;
	settings.autoWarmHands = false;

	WarmHands::ResetMagicSettings(settings);

	CHECK(settings.enableFireMagicWarmth == defaults.enableFireMagicWarmth);
	CHECK(settings.requireActiveCast == defaults.requireActiveCast);
	CHECK(settings.requireBothHands == defaults.requireBothHands);
	CHECK(settings.warmFromActiveFireEffects == defaults.warmFromActiveFireEffects);
	CHECK(settings.logLevel == 0);
	CHECK_FALSE(settings.autoWarmHands);
}

TEST_CASE("ResetAnimationSettings restores only the animation page", "[settings]")
{
	const Settings defaults{};
	Settings       settings{};
	settings.enableAnimation = false;
	settings.autoWarmHands = false;
	settings.idleDelay = 12;
	settings.holdToWarmHands = false;
	settings.holdKeyboardKey = "G";
	settings.holdGamepadButton = "RB";
	settings.holdDuration = 2.0F;
	settings.warmHandsRadius = 250.0F;
	settings.showActivationRange = true;
	settings.warmHandsAtFires = false;
	settings.warmHandsAtForges = false;
	settings.warmHandsAtOtherHeat = false;
	settings.requireActiveCast = true;

	WarmHands::ResetAnimationSettings(settings);

	auto expected = defaults;
	expected.requireActiveCast = true;
	expected.showActivationRange = true;
	CHECK(settings == expected);
}

TEST_CASE("Distances convert between game units and metres", "[settings]")
{
	CHECK(WarmHands::UnitsToMetres(70.0F) == 1.0F);
	CHECK(WarmHands::MetresToUnits(2.0F) == 140.0F);
	CHECK(WarmHands::MetresToUnits(WarmHands::UnitsToMetres(100.0F)) == 100.0F);
}

TEST_CASE("ActiveSettings turns every feature off when the plugin is disabled", "[settings]")
{
	Settings settings{};
	settings.enabled = false;
	settings.warmHandsRadius = 250.0F;
	settings.showActivationRange = true;

	const auto active = WarmHands::ActiveSettings(settings);

	CHECK_FALSE(active.enableFireMagicWarmth);
	CHECK_FALSE(active.showActivationRange);
	CHECK_FALSE(active.autoWarmHands);
	CHECK_FALSE(active.holdToWarmHands);
	CHECK(active.warmHandsRadius == 250.0F);
	CHECK(WarmHands::ActiveSettings(Settings{}) == Settings{});
}

TEST_CASE("ActiveSettings turns the idle off when animation is disabled", "[settings]")
{
	Settings settings{};
	settings.enableAnimation = false;
	settings.showActivationRange = true;

	const auto active = WarmHands::ActiveSettings(settings);

	CHECK_FALSE(active.autoWarmHands);
	CHECK_FALSE(active.holdToWarmHands);
	CHECK(active.showActivationRange);
	CHECK(active.enableFireMagicWarmth);
}
