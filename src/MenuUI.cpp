#include "PCH.h"

#include "Config.h"
#include "HoldInput.h"
#include "KeyNames.h"
#include "MenuUI.h"  // NOLINT(readability-duplicate-include): clang-tidy 21 false positive on Linux clang-cl
#include "RangeOverlay.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"

#include <span>

namespace
{
	template <std::size_t N>
	[[nodiscard]] bool DrawKeyCombo(const char* label, std::string& value, const std::array<WarmHands::KeyName, N>& keys)
	{
		if (!ImGuiMCP::BeginCombo(label, value.c_str())) {
			return false;
		}
		bool changed = false;
		for (const auto& [name, code] : keys) {
			if (ImGuiMCP::Selectable(std::string{ name }.c_str(), WarmHands::NamesMatch(name, value))) {
				value = name;
				changed = true;
			}
		}
		ImGuiMCP::EndCombo();
		return changed;
	}

	WarmHands::Settings& Staged()
	{
		static WarmHands::Settings staged{};
		return staged;
	}

	void ApplyHoldBinding()
	{
		WarmHands::HoldInput::GetSingleton()->Configure(WarmHands::Config::Active());
	}

	void Commit()
	{
		const auto settings = WarmHands::Sanitize(Staged());
		Staged() = settings;
		if (!WarmHands::Config::SaveSettings(settings)) {
			logger::error("Failed to save settings from SKSE Menu Framework");
			Staged() = WarmHands::Config::Current();
			return;
		}
		WarmHands::Config::SetCurrent(settings);
		WarmHands::Config::ApplyLogLevel(settings);
		if (const auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask(ApplyHoldBinding);
		}
		logger::info("Applied settings from SKSE Menu Framework");
	}

	void Toggle(const char* label, bool& value)
	{
		if (ImGuiMCPComponents::ToggleButton(label, &value)) {
			Commit();
		}
	}

	void SliderFloat(const char* label, float& value, float min, float max, const char* format)
	{
		ImGuiMCP::SliderFloat(label, &value, min, max, format, ImGuiMCP::ImGuiSliderFlags_AlwaysClamp);
		if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
			Commit();
		}
	}

	void SliderInt(const char* label, int& value, int min, int max)
	{
		ImGuiMCP::SliderInt(label, &value, min, max, "%d", ImGuiMCP::ImGuiSliderFlags_AlwaysClamp);
		if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
			Commit();
		}
	}

	void InputInt(const char* label, int& value, int min, int max)
	{
		constexpr int kFastStep = 5;
		ImGuiMCP::InputInt(label, &value, 1, kFastStep);
		if (!ImGuiMCP::IsItemDeactivatedAfterEdit()) {
			return;
		}
		value = std::clamp(value, min, max);
		Commit();
	}

	void DistanceSlider(const char* label, float& units, float minUnits, float maxUnits)
	{
		float                                metres = WarmHands::UnitsToMetres(units);
		constexpr ImGuiMCP::ImGuiSliderFlags kFlags = ImGuiMCP::ImGuiSliderFlags_AlwaysClamp | ImGuiMCP::ImGuiSliderFlags_NoRoundToFormat;  // NOLINT(bugprone-signed-bitwise): ImGui flags are signed ints
		if (ImGuiMCP::SliderFloat(label, &metres, WarmHands::UnitsToMetres(minUnits), WarmHands::UnitsToMetres(maxUnits), "%.2f m", kFlags)) {
			units = WarmHands::MetresToUnits(metres);
		}
		if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
			Commit();
		}
	}

	template <std::size_t N>
	void KeyCombo(const char* label, std::string& value, const std::array<WarmHands::KeyName, N>& keys)
	{
		if (DrawKeyCombo(label, value, keys)) {
			Commit();
		}
	}

	void DrawResetButton(void (*resetPage)(WarmHands::Settings&))
	{
		ImGuiMCP::Spacing();
		ImGuiMCP::Separator();
		ImGuiMCP::Spacing();
		if (ImGuiMCP::Button("Reset to defaults")) {
			resetPage(Staged());
			Commit();
		}
	}

	void TextCentered(const char* text)
	{
		const float width = ImGuiMCP::CalcTextSize(text).x;
		const float avail = ImGuiMCP::GetContentRegionAvail().x;
		ImGuiMCP::SetCursorPosX(ImGuiMCP::GetCursorPosX() + std::max(0.0F, (avail - width) / 2.0F));
		ImGuiMCP::TextUnformatted(text);
	}

	void TextBoldItalicCentered(const char* text)
	{
		constexpr float kSlant = 0.2F;
		const float     size = ImGuiMCP::GetFontSize();
		const auto      textSize = ImGuiMCP::CalcTextSize(text);
		const float     width = textSize.x + 1.0F + (kSlant * size);
		const float     avail = ImGuiMCP::GetContentRegionAvail().x;
		ImGuiMCP::SetCursorPosX(ImGuiMCP::GetCursorPosX() + std::max(0.0F, (avail - width) / 2.0F));

		const auto origin = ImGuiMCP::GetCursorScreenPos();
		auto*      drawList = ImGuiMCP::GetWindowDrawList();
		const auto first = static_cast<std::size_t>(drawList->VtxBuffer.Size);
		const auto colour = ImGuiMCP::GetColorU32(ImGuiMCP::ImGuiCol_Text);
		for (const float offset : { 0.0F, 1.0F }) {
			const ImGuiMCP::ImVec2 at{ .x = origin.x + offset, .y = origin.y };
			ImGuiMCP::ImDrawListManager::AddText(drawList, ImGuiMCP::GetFont(), size, at, colour, text);
		}
		ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ .x = width, .y = textSize.y });

		const float baseline = origin.y + size;
		const auto  vertices = std::span{ drawList->VtxBuffer.Data, static_cast<std::size_t>(drawList->VtxBuffer.Size) };
		for (auto& vertex : vertices.subspan(first)) {
			vertex.pos.x += (baseline - vertex.pos.y) * kSlant;
		}
	}

	void LineGap()
	{
		ImGuiMCP::Dummy(ImGuiMCP::ImVec2{ .x = 0.0F, .y = ImGuiMCP::GetFontSize() });
	}

	void DrawTitle()
	{
		constexpr float kTitleScale = 1.6F;
		LineGap();
		ImGuiMCP::SetWindowFontScale(kTitleScale);
		TextBoldItalicCentered("May your road lead you to warm hands");
		ImGuiMCP::SetWindowFontScale(1.0F);

		static const std::string version = [] {
			const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
			return plugin ? "Version " + plugin->GetVersion().string("."sv) : std::string{};
		}();
		TextCentered(version.c_str());
		LineGap();
		ImGuiMCP::Separator();
		LineGap();
	}

	void __stdcall RenderGeneral()
	{
		try {
			auto& s = Staged();

			DrawTitle();
			for (const char* sentence : {
					 "Your own fire magic counts as a heat source for Survival Mode, Survival Mode Improved and Frostfall.",
					 "Cast a Flame Cloak or use fire spells and you warm up as you would beside a small campfire.",
					 "Stand facing a campfire, hearth or forge with empty hands and your character warms their hands over it.",
					 "Move, draw your weapon or tap the activation key to stop.",
					 "Choose when the animation plays on the Animation page and how fire magic warms you on the Magic page.",
				 }) {
				ImGuiMCP::TextWrapped("%s", sentence);
			}

			LineGap();
			ImGuiMCP::SeparatorText("General");
			Toggle("Enable", s.enabled);

			LineGap();
			if (ImGuiMCP::CollapsingHeader("Debugging")) {
				SliderInt("Log level", s.logLevel, WarmHands::kMinLogLevel, WarmHands::kMaxLogLevel);
				Toggle("Show activation range", s.showActivationRange);
			}
		} catch (...) {
			logger::error("RenderGeneral: unhandled exception, skipping frame");
		}
	}

	void __stdcall RenderMagic()
	{
		try {
			auto& s = Staged();

			Toggle("Warm from fire magic", s.enableFireMagicWarmth);
			if (s.enableFireMagicWarmth) {
				ImGuiMCP::Indent();
				Toggle("Only while casting", s.requireActiveCast);
				Toggle("Require both hands", s.requireBothHands);
				Toggle("Active magic effects", s.warmFromActiveFireEffects);
				ImGuiMCP::Unindent();
			}

			DrawResetButton(WarmHands::ResetMagicSettings);
		} catch (...) {
			logger::error("RenderMagic: unhandled exception, skipping frame");
		}
	}

	void DrawIdleSection(WarmHands::Settings& s)
	{
		ImGuiMCP::SeparatorText("Idle animation");
		Toggle("Play automatically when standing near a heat source", s.autoWarmHands);
		if (!s.autoWarmHands) {
			return;
		}
		ImGuiMCP::Indent();
		InputInt("Delay (seconds)", s.idleDelay, WarmHands::kMinIdleDelay, WarmHands::kMaxIdleDelay);
		Toggle("At campfires, hearths and braziers", s.warmHandsAtFires);
		Toggle("At forges and smelters", s.warmHandsAtForges);
		Toggle("At other heat sources such as magical flames", s.warmHandsAtOtherHeat);
		ImGuiMCP::Unindent();
	}

	void DrawManualSection(WarmHands::Settings& s)
	{
		ImGuiMCP::SeparatorText("Manual animation");
		Toggle("Activation key", s.holdToWarmHands);
		if (!s.holdToWarmHands) {
			return;
		}
		ImGuiMCP::Indent();
		ImGuiMCP::TextDisabled("Hold while standing in front of a heat source with empty hands. Tap to stop.");
		KeyCombo("Keyboard key", s.holdKeyboardKey, WarmHands::kKeyboardKeys);
		KeyCombo("Gamepad button", s.holdGamepadButton, WarmHands::kGamepadButtons);
		SliderFloat("Hold duration", s.holdDuration, WarmHands::kMinHoldDuration, WarmHands::kMaxHoldDuration, "%.1fs");
		ImGuiMCP::Unindent();
	}

	void __stdcall RenderAnimation()
	{
		try {
			auto& s = Staged();

			ImGuiMCP::SeparatorText("General");
			Toggle("Enable", s.enableAnimation);

			if (s.enableAnimation) {
				DistanceSlider("Distance from heat source", s.warmHandsRadius, WarmHands::kMinWarmHandsRadius, WarmHands::kMaxWarmHandsRadius);
				LineGap();
				DrawIdleSection(s);
				LineGap();
				DrawManualSection(s);
			}

			DrawResetButton(WarmHands::ResetAnimationSettings);
		} catch (...) {
			logger::error("RenderAnimation: unhandled exception, skipping frame");
		}
	}
}

void WarmHands::MenuUI::Register()
{
	if (!SKSEMenuFramework::IsInstalled()) {
		logger::info("SKSE Menu Framework not installed; settings are INI only");
		return;
	}
	if (!GetMenuFrameworkModule()) {
		logger::warn("SKSE Menu Framework is installed but not loaded; menu disabled");
		return;
	}
	Staged() = Config::Current();
	SKSEMenuFramework::SetSection("Warm Hands");
	SKSEMenuFramework::AddSectionItem("General", RenderGeneral);
	SKSEMenuFramework::AddSectionItem("Animation", RenderAnimation);
	SKSEMenuFramework::AddSectionItem("Magic", RenderMagic);
	RangeOverlay::Register();
	logger::info("SKSE Menu Framework pages registered");
}
