#include "PCH.h"

#include "HeatGeometry.h"
#include "HeatSource.h"

namespace
{
	constexpr auto       kWarmHandsPlugin = "WarmHands.esl"sv;
	constexpr RE::FormID kHandFlameHeat = 0x000800;

	struct HeatList
	{
		std::string_view plugin;
		RE::FormID       localId;
		bool             nearHeatSource;
	};

	constexpr std::array<HeatList, 3> kHeatLists{
		{
			{ .plugin = "ccQDRSSE001-SurvivalMode.esl", .localId = 0x0008AA, .nearHeatSource = true },
			{ .plugin = "Campfire.esm", .localId = 0x028F06, .nearHeatSource = true },
			{ .plugin = "Campfire.esm", .localId = 0x028F02, .nearHeatSource = false },
		},
	};

	constexpr float kLeftoverSearchRadius = 4096.0F;
	constexpr float kFollowDistance = 50.0F;

	constexpr float kHeatSourceReach = 600.0F;
	constexpr float kMaxHeightDifference = 150.0F;
	constexpr float kFacingHalfAngle = std::numbers::pi_v<float> / 4.0F;
	constexpr float kOpeningHalfAngle = std::numbers::pi_v<float> / 3.0F;

	struct FireMesh
	{
		std::string_view fire;
		std::string_view body;  // when set, the fire warms only on the side facing away from it
	};

	constexpr std::array<FireMesh, 4> kFireMeshes{
		{
			{ .fire = "Spout"sv, .body = "Dome"sv },
			{ .fire = "BlacksmithForge01:1"sv, .body = {} },
			{ .fire = "BlacksmithForgeGlow"sv, .body = {} },
			{ .fire = "Flames"sv, .body = {} },
		},
	};

	WarmHands::HeatKind KindOf(const RE::TESBoundObject& base)
	{
		switch (base.GetFormType()) {
		case RE::FormType::Furniture:
			return WarmHands::HeatKind::kForge;
		case RE::FormType::Activator:
			return WarmHands::HeatKind::kOther;
		default:
			return WarmHands::HeatKind::kFire;
		}
	}

	struct FireCircle
	{
		WarmHands::Vec2                position;
		float                          radius;
		std::optional<WarmHands::Vec2> outward;
	};

	std::optional<FireCircle> FindFurnitureFire(RE::TESObjectREFR& ref, const RE::TESBoundObject& base)
	{
		auto* root = ref.Get3D();
		if (!base.Is(RE::FormType::Furniture) || !root) {
			return std::nullopt;
		}
		for (const auto& mesh : kFireMeshes) {
			const auto* fire = root->GetObjectByName(mesh.fire);
			if (!fire) {
				continue;
			}
			const auto& at = fire->worldBound.center;
			FireCircle  found{ .position = { .x = at.x, .y = at.y }, .radius = fire->worldBound.radius, .outward = std::nullopt };
			if (mesh.body.empty()) {
				return found;
			}
			const auto* body = root->GetObjectByName(mesh.body);
			if (!body) {
				return found;
			}
			const auto& centre = body->worldBound.center;
			found.outward = WarmHands::Vec2{ .x = at.x - centre.x, .y = at.y - centre.y };
			return found;
		}
		return std::nullopt;
	}

	struct Measured
	{
		float                        distance;
		bool                         onWarmSide;
		bool                         facing;
		std::vector<WarmHands::Vec2> outline;
		WarmHands::Vec2              centre;
	};

	constexpr int kOutlineSegments = 48;
	constexpr int kCornerSegments = 8;

	constexpr std::array<float, 3> kRayHeights{ 20.0F, 50.0F, 90.0F };

	constexpr float kMaxHitToShape = 105.0F;

	std::optional<WarmHands::Vec2> CastTowards(RE::PlayerCharacter& player, WarmHands::Vec2 target)
	{
		auto* cell = player.GetParentCell();
		auto* world = cell ? cell->GetbhkWorld() : nullptr;
		if (!world) {
			return std::nullopt;
		}
		RE::CFilter filter{};
		player.GetCollisionFilterInfo(filter);
		filter.SetCollisionLayer(RE::COL_LAYER::kLOS);
		const float           scale = RE::bhkWorld::GetWorldScale();
		const auto            feet = player.GetPosition();
		const WarmHands::Vec2 toTarget{ .x = target.x - feet.x, .y = target.y - feet.y };
		const float           length = WarmHands::Length(toTarget);
		std::optional<float>  distance;
		for (const float height : kRayHeights) {
			const RE::NiPoint3 from{ feet.x, feet.y, feet.z + height };
			const RE::NiPoint3 to{ target.x, target.y, feet.z + height };
			RE::bhkPickData    pick;
			pick.rayInput.from = RE::hkVector4(from * scale);
			pick.rayInput.to = RE::hkVector4(to * scale);
			pick.rayInput.filterInfo = filter;
			{
				const RE::BSReadLockGuard lock{ world->worldLock };
				world->PickObject(pick);
			}
			if (pick.rayOutput.HasHit()) {
				const float hit = pick.rayOutput.hitFraction * length;
				distance = distance ? std::min(*distance, hit) : hit;
			}
		}
		if (!distance || length == 0.0F) {
			return std::nullopt;
		}
		const float fraction = *distance / length;
		return WarmHands::Vec2{ .x = feet.x + (toTarget.x * fraction), .y = feet.y + (toTarget.y * fraction) };
	}

	std::vector<WarmHands::Vec2> CircleOutline(const FireCircle& fire, float reach)
	{
		const float outer = fire.radius + reach;
		if (!fire.outward) {
			return WarmHands::Arc(fire.position, outer, 0.0F, 2.0F * std::numbers::pi_v<float>, kOutlineSegments);
		}
		const float bearing = WarmHands::BearingOf(*fire.outward);
		auto        points = WarmHands::Arc(fire.position, outer, bearing - kOpeningHalfAngle, bearing + kOpeningHalfAngle, kOutlineSegments / 3);
		points.push_back(fire.position);
		return points;
	}

	Measured MeasureFireCircle(RE::PlayerCharacter& player, const RE::TESObjectREFR& ref, const FireCircle& fire, float reach)
	{
		const auto            playerPos = player.GetPosition();
		const WarmHands::Vec2 fromFire{ .x = playerPos.x - fire.position.x, .y = playerPos.y - fire.position.y };
		const WarmHands::Vec2 toFire{ .x = -fromFire.x, .y = -fromFire.y };
		const bool            onWarmSide = !fire.outward || WarmHands::IsWithinAngle(*fire.outward, fromFire, kOpeningHalfAngle);
		const bool            facing = WarmHands::IsFacing(player.GetAngleZ(), toFire, kFacingHalfAngle);
		const float           distance = std::max(WarmHands::Length(fromFire) - fire.radius, 0.0F);
		logger::debug("Heat source {:08X}: fire {:.0f} units, warmSide={} facing={}", ref.GetFormID(), distance, onWarmSide, facing);
		return { .distance = distance, .onWarmSide = onWarmSide, .facing = facing, .outline = CircleOutline(fire, reach), .centre = fire.position };
	}

	std::optional<Measured> Measure(RE::PlayerCharacter& player, RE::TESObjectREFR& ref, const RE::TESBoundObject& base, float reach)
	{
		const auto playerPos = player.GetPosition();
		const auto refPos = ref.GetPosition();
		if (std::abs(playerPos.z - refPos.z) > kMaxHeightDifference) {
			logger::debug("Heat source {:08X} skipped: height difference {:.0f}", ref.GetFormID(), playerPos.z - refPos.z);
			return std::nullopt;
		}
		if (const auto fire = FindFurnitureFire(ref, base)) {
			return MeasureFireCircle(player, ref, *fire, reach);
		}
		const float                scale = ref.GetScale();
		const auto&                bounds = base.boundData;
		const WarmHands::Footprint footprint{
			.minX = static_cast<float>(bounds.boundMin.x) * scale,
			.minY = static_cast<float>(bounds.boundMin.y) * scale,
			.maxX = static_cast<float>(bounds.boundMax.x) * scale,
			.maxY = static_cast<float>(bounds.boundMax.y) * scale,
		};
		const WarmHands::Vec2 origin{ .x = refPos.x, .y = refPos.y };
		if (KindOf(base) == WarmHands::HeatKind::kFire) {
			return MeasureFireCircle(player, ref, { .position = origin, .radius = WarmHands::FlameRadius(footprint), .outward = std::nullopt }, reach);
		}
		const WarmHands::Vec2 offset{ .x = playerPos.x - refPos.x, .y = playerPos.y - refPos.y };
		const auto            toEdge = WarmHands::ToNearestEdge(offset, ref.GetAngleZ(), footprint);
		const bool            facing = WarmHands::IsFacingFootprint(player.GetAngleZ(), offset, ref.GetAngleZ(), footprint, kFacingHalfAngle);
		logger::debug("Heat source {:08X} (base {:08X}): edge {:.0f} units, facing={}", ref.GetFormID(), base.GetFormID(), WarmHands::Length(toEdge), facing);
		auto outline = WarmHands::RoundedOutline(footprint, reach, kCornerSegments);
		for (auto& point : outline) {
			const auto world = WarmHands::RotateToWorld(point, ref.GetAngleZ());
			point = { .x = origin.x + world.x, .y = origin.y + world.y };
		}
		return Measured{ .distance = WarmHands::Length(toEdge), .onWarmSide = true, .facing = facing, .outline = std::move(outline), .centre = origin };
	}

	constexpr float kStayMargin = 35.0F;

	float BodyExtent(RE::PlayerCharacter& player, WarmHands::Vec2 direction, float fallback)
	{
		const auto* controller = player.GetCharController();
		if (!controller) {
			return fallback;
		}
		const auto& bound = controller->bumperCollisionBound;
		return WarmHands::BoxExtentAlong(direction, player.GetAngleZ(), { .x = bound.center.x, .y = bound.center.y }, bound.extents.x, bound.extents.y);
	}

	struct Reach
	{
		std::optional<WarmHands::Vec2> hit;
		float                          gap{ 0.0F };
		float                          limit{ 0.0F };
		bool                           inReach{ false };
	};

	Reach CheckReach(RE::PlayerCharacter& player, const Measured& measured, float setting, float bodyRadius, bool castRays)
	{
		const auto            hit = castRays ? CastTowards(player, measured.centre) : std::nullopt;
		const auto            feet = player.GetPosition();
		const WarmHands::Vec2 from{ .x = feet.x, .y = feet.y };
		if (!hit) {
			const float limit = setting + bodyRadius;
			return { .hit = std::nullopt, .gap = measured.distance, .limit = limit, .inReach = measured.onWarmSide && measured.distance <= limit };
		}
		const WarmHands::Vec2 toHit{ .x = hit->x - from.x, .y = hit->y - from.y };
		const float           gap = std::max(WarmHands::Length(toHit) - BodyExtent(player, toHit, bodyRadius), 0.0F);
		const bool            inReach = measured.onWarmSide && gap <= setting && measured.distance - WarmHands::Length(toHit) <= kMaxHitToShape;
		return { .hit = hit, .gap = gap, .limit = setting, .inReach = inReach };
	}

	WarmHands::HeatSource::Outline ToOutline(const Measured& measured, const Reach& reach, const RE::TESObjectREFR& ref, const RE::NiPoint3& feet)
	{
		const std::string_view name = ref.GetName();
		return {
			.points = measured.outline,
			.height = feet.z,
			.label = std::format("{} {}{:.2f} m / {:.2f} m", name.empty() ? std::format("{:08X}", ref.GetFormID()) : std::string{ name },
				reach.hit ? "" : "no collision ", WarmHands::UnitsToMetres(reach.gap), WarmHands::UnitsToMetres(reach.limit)),
			.labelAt = measured.centre,
			.from = { .x = feet.x, .y = feet.y },
			.hit = reach.hit,
			.inReach = reach.inReach,
			.facing = measured.facing,
		};
	}

	struct PublishedOutlines
	{
		std::mutex                                                         mutex;
		std::shared_ptr<const std::vector<WarmHands::HeatSource::Outline>> outlines{ std::make_shared<const std::vector<WarmHands::HeatSource::Outline>>() };
	};

	PublishedOutlines& GetPublishedOutlines()
	{
		static PublishedOutlines published;
		return published;
	}

	void Publish(std::vector<WarmHands::HeatSource::Outline> outlines)
	{
		auto&                  published = GetPublishedOutlines();
		const std::scoped_lock lock{ published.mutex };
		published.outlines = std::make_shared<const std::vector<WarmHands::HeatSource::Outline>>(std::move(outlines));
	}

	struct State
	{
		RE::TESObjectACTI*            activator{ nullptr };
		std::vector<RE::BGSListForm*> nearHeatLists;
		RE::ObjectRefHandle           reference;
	};

	State& GetState()
	{
		static State state;
		return state;
	}

	void RemoveReference(RE::TESObjectREFR& ref)
	{
		ref.Disable();
		ref.SetDelete(true);
	}
}

bool WarmHands::HeatSource::Initialize()
{
	auto* data = RE::TESDataHandler::GetSingleton();
	if (!data) {
		return false;
	}
	auto& state = GetState();
	state.activator = data->LookupForm<RE::TESObjectACTI>(kHandFlameHeat, kWarmHandsPlugin);
	if (!state.activator) {
		logger::error("WarmHands.esl is not loaded; fire magic cannot warm the player");
		return false;
	}

	for (const auto& entry : kHeatLists) {
		auto* list = data->LookupForm<RE::BGSListForm>(entry.localId, entry.plugin);
		if (!list) {
			continue;
		}
		if (!list->HasForm(state.activator)) {
			list->AddForm(state.activator);
		}
		if (entry.nearHeatSource) {
			state.nearHeatLists.push_back(list);
		}
		logger::info("Registered hand flame in heat list {:08X} ({})", list->GetFormID(), entry.plugin);
	}

	if (state.nearHeatLists.empty()) {
		logger::warn("No Survival Mode or Campfire heat lists found; fire magic warmth and the warm hands idle are inactive");
	}
	return !state.nearHeatLists.empty();
}

void WarmHands::HeatSource::Update(RE::PlayerCharacter& player, bool active)
{
	auto&      state = GetState();
	const auto ref = state.reference.get();
	if (!active) {
		if (ref) {
			RemoveReference(*ref);
		}
		state.reference.reset();
		return;
	}
	if (ref && !ref->IsDisabled()) {
		const bool left = ref->GetParentCell() != player.GetParentCell() || ref->GetPosition().GetDistance(player.GetPosition()) > kFollowDistance;
		if (left) {
			ref->MoveTo(&player);
		}
		return;
	}
	if (ref) {
		RemoveReference(*ref);
	}
	const auto placed = player.PlaceObjectAtMe(state.activator, false);
	if (!placed) {
		logger::warn("Could not place the hand-flame heat source");
		return;
	}
	state.reference = placed->GetHandle();
}

namespace
{
	const RE::TESBoundObject* LoadedHeatSourceBase(const State& state, RE::TESObjectREFR* ref)
	{
		const auto* base = ref ? ref->GetBaseObject() : nullptr;
		if (!base || base == state.activator) {
			return nullptr;
		}
		const bool isHeatSource = std::ranges::any_of(state.nearHeatLists, [base](const RE::BGSListForm* list) { return list->HasForm(base); });
		if (!isHeatSource) {
			return nullptr;
		}
		if (ref->IsDisabled() || !ref->Is3DLoaded()) {
			logger::debug("Heat source {:08X} skipped: disabled={} 3DLoaded={}", ref->GetFormID(), ref->IsDisabled(), ref->Is3DLoaded());
			return nullptr;
		}
		return base;
	}
}

WarmHands::HeatSource::NearHeat WarmHands::HeatSource::FindHeat(RE::PlayerCharacter& player, const Settings& settings, bool idlePlaying)
{
	auto*    tes = RE::TES::GetSingleton();
	auto&    state = GetState();
	NearHeat found{};
	if (!tes || state.nearHeatLists.empty()) {
		return found;
	}
	// The setting is the gap between the player's body and the heat, not their centre.
	const float                      bodyRadius = player.GetBoundRadius();
	const float                      setting = settings.warmHandsRadius + (idlePlaying ? kStayMargin : 0.0F);
	const float                      reach = setting + bodyRadius;
	const auto                       feet = player.GetPosition();
	std::vector<HeatSource::Outline> outlines;
	tes->ForEachReferenceInRange(&player, settings.warmHandsRadius + kHeatSourceReach, [&](RE::TESObjectREFR* ref) {
		const auto* base = LoadedHeatSourceBase(state, ref);
		if (!base) {
			return RE::BSContainer::ForEachResult::kContinue;
		}
		const auto measured = Measure(player, *ref, *base, reach);
		if (!measured) {
			return RE::BSContainer::ForEachResult::kContinue;
		}
		const bool castRays = measured->onWarmSide && (measured->facing || settings.showActivationRange);
		const auto checked = CheckReach(player, *measured, setting, bodyRadius, castRays);
		logger::debug("Heat source {:08X}: {} gap {:.0f} units", ref->GetFormID(), checked.hit ? "collision" : "shape", checked.gap);
		if (settings.showActivationRange) {
			outlines.push_back(ToOutline(*measured, checked, *ref, feet));
		}
		if (!checked.inReach || !measured->facing) {
			return RE::BSContainer::ForEachResult::kContinue;
		}
		found.any = true;
		found.autoEnabled = found.autoEnabled || WarmsHandsAt(KindOf(*base), settings);
		const bool done = found.autoEnabled && !settings.showActivationRange;
		return done ? RE::BSContainer::ForEachResult::kStop : RE::BSContainer::ForEachResult::kContinue;
	});
	Publish(std::move(outlines));
	return found;
}

void WarmHands::HeatSource::RemoveLeftovers(RE::PlayerCharacter& player)
{
	auto* tes = RE::TES::GetSingleton();
	auto& state = GetState();
	if (!tes || !state.activator) {
		return;
	}
	state.reference.reset();
	int removed = 0;
	tes->ForEachReferenceInRange(&player, kLeftoverSearchRadius, [&](RE::TESObjectREFR* ref) {
		if (ref && ref->GetBaseObject() == state.activator && !ref->IsDisabled()) {
			RemoveReference(*ref);
			++removed;
		}
		return RE::BSContainer::ForEachResult::kContinue;
	});
	if (removed > 0) {
		logger::info("Removed {} leftover hand-flame heat source(s)", removed);
	}
}

std::shared_ptr<const std::vector<WarmHands::HeatSource::Outline>> WarmHands::HeatSource::Outlines()
{
	auto&                  published = GetPublishedOutlines();
	const std::scoped_lock lock{ published.mutex };
	return published.outlines;
}
