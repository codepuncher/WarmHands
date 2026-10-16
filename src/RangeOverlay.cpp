#include "PCH.h"

#include "Config.h"
#include "HeatSource.h"
#include "RangeOverlay.h"  // NOLINT(readability-duplicate-include): clang-tidy 21 false positive on Linux clang-cl
#include "SKSEMCP/SKSEMenuFramework.hpp"

namespace
{
	constexpr float kLabelHeight = 100.0F;
	constexpr float kLineThickness = 2.0F;
	constexpr float kZeroTolerance = 1e-5F;
	constexpr float kHitMarkerRadius = 5.0F;
	constexpr int   kHitMarkerSegments = 12;

	constexpr ImGuiMCP::ImU32 kInReachColour = IM_COL32(80, 220, 80, 255);
	constexpr ImGuiMCP::ImU32 kFacingAwayColour = IM_COL32(240, 200, 60, 255);
	constexpr ImGuiMCP::ImU32 kOutOfReachColour = IM_COL32(230, 70, 70, 255);

	std::optional<ImGuiMCP::ImVec2> ToScreen(RE::NiCamera& camera, const RE::NiPoint3& point, ImGuiMCP::ImVec2 display)
	{
		float x = 0.0F;
		float y = 0.0F;
		float depth = 0.0F;
		if (!camera.WorldPtToScreenPt3(point, x, y, depth, kZeroTolerance) || depth < 0.0F) {
			return std::nullopt;
		}
		return ImGuiMCP::ImVec2{ .x = x * display.x, .y = (1.0F - y) * display.y };
	}

	ImGuiMCP::ImU32 ColourOf(const WarmHands::HeatSource::Outline& outline)
	{
		if (!outline.inReach) {
			return kOutOfReachColour;
		}
		return outline.facing ? kInReachColour : kFacingAwayColour;
	}

	void DrawHit(ImGuiMCP::ImDrawList* list, RE::NiCamera& camera, ImGuiMCP::ImVec2 display, const WarmHands::HeatSource::Outline& outline, ImGuiMCP::ImU32 colour)
	{
		if (!outline.hit) {
			return;
		}
		const auto from = ToScreen(camera, { outline.from.x, outline.from.y, outline.height }, display);
		const auto hit = ToScreen(camera, { outline.hit->x, outline.hit->y, outline.height }, display);
		if (!from || !hit) {
			return;
		}
		ImGuiMCP::ImDrawListManager::AddLine(list, *from, *hit, colour, kLineThickness);
		ImGuiMCP::ImDrawListManager::AddCircleFilled(list, *hit, kHitMarkerRadius, colour, kHitMarkerSegments);
	}

	void DrawOutline(ImGuiMCP::ImDrawList* list, RE::NiCamera& camera, ImGuiMCP::ImVec2 display, const WarmHands::HeatSource::Outline& outline)
	{
		const auto colour = ColourOf(outline);
		const auto count = outline.points.size();
		for (std::size_t i = 0; i < count; ++i) {
			const auto& from = outline.points[i];
			const auto& to = outline.points[(i + 1) % count];
			const auto  start = ToScreen(camera, { from.x, from.y, outline.height }, display);
			const auto  end = ToScreen(camera, { to.x, to.y, outline.height }, display);
			if (start && end) {
				ImGuiMCP::ImDrawListManager::AddLine(list, *start, *end, colour, kLineThickness);
			}
		}
		DrawHit(list, camera, display, outline, colour);
		const auto label = ToScreen(camera, { outline.labelAt.x, outline.labelAt.y, outline.height + kLabelHeight }, display);
		if (label) {
			ImGuiMCP::ImDrawListManager::AddText(list, *label, colour, outline.label.c_str());
		}
	}

	void __stdcall Draw()
	{
		try {
			if (!WarmHands::Config::Active().showActivationRange) {
				return;
			}
			auto*       camera = RE::Main::WorldRootCamera();
			auto*       list = ImGuiMCP::GetBackgroundDrawList();
			const auto* io = ImGuiMCP::GetIO();
			if (!camera || !list || !io) {
				return;
			}
			const auto outlines = WarmHands::HeatSource::Outlines();
			for (const auto& outline : *outlines) {
				DrawOutline(list, *camera, io->DisplaySize, outline);
			}
		} catch (...) {
			logger::error("RangeOverlay: unhandled exception, skipping frame");
		}
	}
}

void WarmHands::RangeOverlay::Register()
{
	if (REL::Module::IsVR()) {
		return;
	}
	using Register = SKSEMenuFramework::Model::RegisterHudElementFuction;
	const auto registerHudElement = SKSEMenuFramework::Model::Internal::GetFunction<Register>("RegisterHudElement");
	if (!registerHudElement) {
		logger::warn("SKSE Menu Framework has no HUD elements; activation range overlay unavailable");
		return;
	}
	registerHudElement(Draw);
}
