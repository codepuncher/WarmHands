#include "PCH.h"

#include "Config.h"

#include <concepts>
#include <type_traits>

namespace
{
	constexpr auto kIniPath = R"(Data\SKSE\Plugins\WarmHands.ini)";
	constexpr auto kGeneral = "General";
	constexpr auto kFireMagic = "FireMagic";
	constexpr auto kAnimation = "Animation";
	constexpr auto kDebugging = "Debugging";

	struct Shared
	{
		std::mutex          mutex;
		WarmHands::Settings settings{};
	};

	Shared& GetShared()
	{
		static Shared shared;
		return shared;
	}

	void LoadValue(const CSimpleIniA& ini, const char* section, const char* key, bool& value)
	{
		value = ini.GetBoolValue(section, key, value);
	}

	void LoadValue(const CSimpleIniA& ini, const char* section, const char* key, float& value)
	{
		value = static_cast<float>(ini.GetDoubleValue(section, key, value));
	}

	void LoadValue(const CSimpleIniA& ini, const char* section, const char* key, int& value)
	{
		value = static_cast<int>(ini.GetLongValue(section, key, value));
	}

	void LoadValue(const CSimpleIniA& ini, const char* section, const char* key, std::string& value)
	{
		const std::string fallback = value;
		value = ini.GetValue(section, key, fallback.c_str());
	}

	bool SaveValue(CSimpleIniA& ini, const char* section, const char* key, bool value)
	{
		return ini.SetBoolValue(section, key, value) >= SI_OK;
	}

	bool SaveValue(CSimpleIniA& ini, const char* section, const char* key, float value)
	{
		const auto text = fmt::format("{:g}", value);
		return ini.SetValue(section, key, text.c_str()) >= SI_OK;
	}

	bool SaveValue(CSimpleIniA& ini, const char* section, const char* key, int value)
	{
		return ini.SetLongValue(section, key, value) >= SI_OK;
	}

	bool SaveValue(CSimpleIniA& ini, const char* section, const char* key, const std::string& value)
	{
		return ini.SetValue(section, key, value.c_str()) >= SI_OK;
	}

	template <typename S, typename Visitor>
		requires std::same_as<std::remove_const_t<S>, WarmHands::Settings>
	void ForEachSetting(S& settings, Visitor visit)
	{
		visit(kGeneral, "bEnabled", settings.enabled);
		visit(kFireMagic, "bEnableFireMagicWarmth", settings.enableFireMagicWarmth);
		visit(kFireMagic, "bRequireActiveCast", settings.requireActiveCast);
		visit(kFireMagic, "bRequireBothHands", settings.requireBothHands);
		visit(kFireMagic, "bWarmFromActiveFireEffects", settings.warmFromActiveFireEffects);
		visit(kAnimation, "bEnableAnimation", settings.enableAnimation);
		visit(kAnimation, "bAutoWarmHands", settings.autoWarmHands);
		visit(kAnimation, "iIdleDelay", settings.idleDelay);
		visit(kAnimation, "bHoldToWarmHands", settings.holdToWarmHands);
		visit(kAnimation, "sHoldKeyboardKey", settings.holdKeyboardKey);
		visit(kAnimation, "sHoldGamepadButton", settings.holdGamepadButton);
		visit(kAnimation, "fHoldDuration", settings.holdDuration);
		visit(kAnimation, "fWarmHandsRadius", settings.warmHandsRadius);
		visit(kAnimation, "bWarmHandsAtFires", settings.warmHandsAtFires);
		visit(kAnimation, "bWarmHandsAtForges", settings.warmHandsAtForges);
		visit(kAnimation, "bWarmHandsAtOtherHeat", settings.warmHandsAtOtherHeat);
		visit(kDebugging, "iLogLevel", settings.logLevel);
		visit(kDebugging, "bShowActivationRange", settings.showActivationRange);
	}
}

WarmHands::Settings WarmHands::Config::LoadSettings()
{
	CSimpleIniA ini;
	const auto  rc = ini.LoadFile(kIniPath);
	if (rc < SI_OK) {
		logger::warn("WarmHands.ini not found or unreadable (rc={}), using defaults", static_cast<int>(rc));
	}

	Settings raw{};
	ForEachSetting(raw, [&ini](const char* section, const char* key, auto& value) { LoadValue(ini, section, key, value); });

	const auto settings = Sanitize(raw);
	if (settings != raw) {
		logger::warn("WarmHands.ini has out-of-range values; using sanitised values");
	}
	return settings;
}

bool WarmHands::Config::SaveSettings(const Settings& settings)
{
	CSimpleIniA ini;
	ini.SetSpaces(false);
	const auto loadRc = ini.LoadFile(kIniPath);
	if (loadRc < SI_OK && loadRc != SI_FILE) {
		logger::error("Could not parse existing WarmHands.ini (rc={}); not saving, to keep it intact", static_cast<int>(loadRc));
		return false;
	}

	bool stored = true;
	ForEachSetting(settings, [&ini, &stored](const char* section, const char* key, const auto& value) {
		if (!SaveValue(ini, section, key, value)) {
			logger::error("Could not store {} in WarmHands.ini", key);
			stored = false;
		}
	});
	if (!stored) {
		return false;
	}

	const auto rc = ini.SaveFile(kIniPath);
	if (rc < SI_OK) {
		logger::error("Failed to write WarmHands.ini (rc={})", static_cast<int>(rc));
		return false;
	}
	return true;
}

WarmHands::Settings WarmHands::Config::Current()
{
	auto&                  shared = GetShared();
	const std::scoped_lock lock{ shared.mutex };
	return shared.settings;
}

WarmHands::Settings WarmHands::Config::Active()
{
	return ActiveSettings(Current());
}

void WarmHands::Config::SetCurrent(const Settings& settings)
{
	auto&                  shared = GetShared();
	const std::scoped_lock lock{ shared.mutex };
	shared.settings = settings;
}

void WarmHands::Config::ApplyLogLevel(const Settings& settings)
{
	spdlog::set_level(static_cast<spdlog::level::level_enum>(settings.logLevel));
}
