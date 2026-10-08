#include "session.h"
#include "sprites.h"
#include "minimap.h"
#include "tile.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using rme::core::EditorSession;
using rme::core::ItemDatabase;
using rme::core::Sprite;
using rme::core::SpriteSheet;

namespace {

int Fail(const char* message) {
	std::fprintf(stderr, "%s\n", message);
	return 1;
}

} // namespace

int main() {
	const auto dir = std::filesystem::temp_directory_path();
	const auto sample = (dir / "rme_phase3_sample.otbm").string();
	const auto saved = (dir / "rme_phase3_roundtrip.otbm").string();
	const auto dat_path = (dir / "rme_phase3_Tibia.dat").string();
	const auto spr_path = (dir / "rme_phase3_Tibia.spr").string();

	EditorSession session;
	if (!session.createSampleAssets(dat_path, spr_path)) {
		std::fprintf(stderr, "createSampleAssets failed: %s\n", session.lastError().c_str());
		return 1;
	}
	if (!session.assets().dat_loaded || session.items().size() != 6) {
		return Fail("expected 6 sample items (100-105)");
	}
	const auto* grass = session.items().get(100);
	const auto* water = session.items().get(102);
	const auto* wall = session.items().get(103);
	const auto* box = session.items().get(105);
	if (!grass || !grass->ground || grass->sprite_id != 1) {
		return Fail("item 100 should be grass with sprite 1");
	}
	if (!water || !water->not_walkable || water->sprite_id != 3) {
		return Fail("item 102 should be unwalkable water");
	}
	if (!wall || !wall->not_walkable || !wall->block_projectile) {
		return Fail("item 103 should block walk and projectiles");
	}
	if (!box || !box->container || !box->pickupable) {
		return Fail("item 105 should be a pickupable container");
	}
	if (session.sprites().count() != 6 || !session.sprites().get(1) || session.sprites().get(1)->empty()) {
		return Fail("sample .spr should decode sprites 1-6");
	}

	const auto* grass_sprite = session.sprites().get(1);
	Sprite roundtrip;
	const auto encoded = SpriteSheet::EncodeSprite(grass_sprite->rgba);
	if (!SpriteSheet::DecodeSprite(encoded.data(), encoded.size(), roundtrip)) {
		return Fail("sprite encode/decode failed");
	}
	if (roundtrip.rgba.size() != grass_sprite->rgba.size()) {
		return Fail("sprite roundtrip size mismatch");
	}
	int opaque = 0;
	for (std::size_t i = 3; i < grass_sprite->rgba.size(); i += 4) {
		if (grass_sprite->rgba[i] == 255) {
			++opaque;
		}
	}
	if (opaque < 32 * 32 / 2) {
		return Fail("grass sprite should be mostly opaque");
	}

	ItemDatabase reloaded_items;
	rme::core::ClientAssetsInfo info;
	if (!reloaded_items.load(dat_path, info) || reloaded_items.size() != 6) {
		return Fail("dat reload failed");
	}
	if (!reloaded_items.get(104) || !reloaded_items.get(104)->pickupable || reloaded_items.get(104)->sprite_id != 5) {
		return Fail("reloaded flower lost pickupable/sprite");
	}

	if (!session.createSampleMap(sample)) {
		std::fprintf(stderr, "createSampleMap failed: %s\n", session.lastError().c_str());
		return 1;
	}
	if (session.map().tileCount() != 34) {
		std::fprintf(stderr, "expected 34 sample tiles, got %zu\n", session.map().tileCount());
		return 1;
	}
	const auto* center = session.map().getTile(Position(100, 100, session.floor()));
	if (!center || !center->getGround() || center->getItems().size() != 2 || center->getItems().front().getID() != 105
		|| center->getItems().back().getID() != 104) {
		return Fail("sample map center should stack crate then cover flower");
	}
	if (center->getItems().front().getActionID() != 1000 || center->getItems().front().getUniqueID() != 2000
		|| center->getItems().front().getText() != "Phase 8 crate") {
		return Fail("sample crate should have AID/UID/text");
	}
	if (center->getItems().front().getContents().size() != 1
		|| center->getItems().front().getContents().front().getID() != 104
		|| center->getItems().front().getContents().front().getCount() != 3) {
		return Fail("sample crate should contain three flowers");
	}
	const auto* portal = session.map().getTile(Position(99, 102, session.floor()));
	if (!portal || portal->getItems().empty() || !portal->getItems().front().hasDestination()
		|| portal->getItems().front().getDestination() != Position(100, 100, rme::MapGroundLayer + 1)) {
		return Fail("sample portal flower should teleport to the cave");
	}
	bool saw_pattern = false;
	for (const auto& [_, tile] : session.map().tiles()) {
		if (tile.getGround() && tile.getGround()->getID() > 100) {
			saw_pattern = true;
			break;
		}
	}
	if (!saw_pattern) {
		return Fail("sample map should use more than item 100");
	}

	const Position paint_pos(101, 101, session.floor());
	session.paintGround(paint_pos, 351);
	if (!session.canUndo()) {
		return Fail("paint should create an undo step");
	}
	session.undo();
	if (const auto* tile = session.map().getTile(paint_pos); tile && tile->getGround() && tile->getGround()->getID() == 351) {
		return Fail("undo did not restore the painted tile");
	}
	session.redo();
	const auto* painted = session.map().getTile(paint_pos);
	if (!painted || !painted->getGround() || painted->getGround()->getID() != 351) {
		return Fail("redo did not reapply the painted tile");
	}

	if (!session.saveOtbm(saved)) {
		std::fprintf(stderr, "saveOtbm failed: %s\n", session.lastError().c_str());
		return 1;
	}

	EditorSession reloaded;
	if (!reloaded.loadOtbm(saved)) {
		std::fprintf(stderr, "reload failed: %s\n", reloaded.lastError().c_str());
		return 1;
	}
	if (reloaded.map().tileCount() != session.map().tileCount()) {
		std::fprintf(stderr, "roundtrip tile count mismatch %zu vs %zu\n", reloaded.map().tileCount(), session.map().tileCount());
		return 1;
	}
	const auto* again = reloaded.map().getTile(paint_pos);
	if (!again || !again->getGround() || again->getGround()->getID() != 351) {
		return Fail("roundtrip lost painted item 351");
	}
	if (reloaded.map().towns().size() != 1 || reloaded.map().waypoints().size() != 1) {
		return Fail("roundtrip lost town/waypoint metadata");
	}

	EditorSession tools;
	if (!tools.createSampleAssets(dat_path, spr_path) || !tools.createSampleMap(sample)) {
		return Fail("brush fixture failed");
	}

	const Position overlay_pos(99, 99, tools.floor());
	const uint16_t overlay_ground = tools.map().getTile(overlay_pos) && tools.map().getTile(overlay_pos)->getGround()
		? tools.map().getTile(overlay_pos)->getGround()->getID()
		: 100;
	tools.setBrushKind(rme::core::BrushKind::Overlay);
	tools.setBrushId(104);
	tools.beginStroke();
	tools.strokeAt(overlay_pos);
	tools.endStroke();
	const auto* overlay_tile = tools.map().getTile(overlay_pos);
	if (!overlay_tile || !overlay_tile->getGround() || overlay_tile->getGround()->getID() != overlay_ground) {
		return Fail("overlay brush replaced ground");
	}
	if (overlay_tile->getItems().size() != 1 || overlay_tile->getItems().front().getID() != 104) {
		return Fail("overlay brush should add a flower");
	}

	tools.setBrushKind(rme::core::BrushKind::Ground);
	tools.setBrushId(102);
	tools.beginStroke();
	tools.strokeAt(Position(110, 110, tools.floor()));
	tools.strokeAt(Position(111, 110, tools.floor()));
	tools.strokeAt(Position(112, 110, tools.floor()));
	tools.endStroke();
	if (tools.history().undoDepth() < 2) {
		return Fail("drag stroke should commit as one undo step after the overlay");
	}
	const std::size_t before_undo = tools.map().tileCount();
	tools.undo();
	if (tools.map().getTile(Position(110, 110, tools.floor()))) {
		return Fail("undo should remove the whole water stroke");
	}
	tools.redo();
	if (tools.map().tileCount() != before_undo) {
		return Fail("redo did not restore the water stroke");
	}
	if (!tools.map().getTile(Position(112, 110, tools.floor()))
		|| !tools.map().getTile(Position(112, 110, tools.floor()))->hasGround()
		|| tools.map().getTile(Position(112, 110, tools.floor()))->getGround()->getID() != 102) {
		return Fail("stroke missed the last water tile");
	}

	tools.setBrushId(101);
	tools.fillAt(Position(110, 110, tools.floor()));
	if (!tools.map().getTile(Position(111, 110, tools.floor()))
		|| tools.map().getTile(Position(111, 110, tools.floor()))->getGround()->getID() != 101) {
		return Fail("fill should replace connected water");
	}

	tools.setBrushKind(rme::core::BrushKind::Eraser);
	tools.beginStroke();
	tools.strokeAt(overlay_pos);
	tools.endStroke();
	const auto* erased = tools.map().getTile(overlay_pos);
	if (!erased || !erased->getItems().empty()) {
		return Fail("eraser should pop the overlay item first");
	}
	if (!erased->hasGround() || erased->getGround()->getID() != overlay_ground) {
		return Fail("first erase should keep the ground");
	}

	tools.setBrushKind(rme::core::BrushKind::Ground);
	tools.setBrushSize(3);
	if (tools.hoverFootprint(Position(120, 120, tools.floor())).size() != 9) {
		return Fail("3x3 brush footprint should be 9 tiles");
	}
	tools.setBrushId(100);
	tools.beginStroke();
	tools.strokeAt(Position(120, 120, tools.floor()));
	tools.endStroke();
	if (!tools.map().getTile(Position(121, 121, tools.floor()))
		|| tools.map().getTile(Position(121, 121, tools.floor()))->getGround()->getID() != 100) {
		return Fail("3x3 ground stroke should paint the corner");
	}

	tools.selection().begin(Position(120, 120, tools.floor()));
	tools.selection().update(Position(121, 121, tools.floor()));
	tools.selection().finish();
	if (tools.selection().size() != 4) {
		return Fail("2x2 selection should contain 4 tiles");
	}
	tools.copySelection();
	if (tools.clipboardSize() != 4) {
		return Fail("copy should keep the 2x2 grass block");
	}
	tools.pasteAt(Position(130, 130, tools.floor()));
	if (!tools.map().getTile(Position(131, 131, tools.floor()))
		|| tools.map().getTile(Position(131, 131, tools.floor()))->getGround()->getID() != 100) {
		return Fail("paste should restore the copied grass");
	}
	tools.deleteSelection();
	if (tools.map().getTile(Position(120, 120, tools.floor()))) {
		return Fail("delete selection should clear the source tiles");
	}

	if (!tools.pickAt(Position(130, 130, tools.floor())) || tools.brushId() != 100) {
		return Fail("eyedropper should pick pasted grass");
	}

	const auto& mm = tools.minimap();
	if (mm.width() != 256 || mm.height() != 256) {
		return Fail("minimap should match map size");
	}
	const uint32_t grass_px = mm.pixel(102, 102);
	const uint32_t water_px = mm.pixel(100, 100);
	const uint8_t grass_g = static_cast<uint8_t>((grass_px >> 8) & 0xFF);
	const uint8_t water_b = static_cast<uint8_t>((water_px >> 16) & 0xFF);
	if (grass_g < 120) {
		return Fail("grass minimap pixel should be green");
	}
	if (water_b < 80) {
		return Fail("water minimap pixel should be blue");
	}

	const auto* pz_tile = tools.map().getTile(Position(100, 100, tools.floor()));
	if (!pz_tile || !pz_tile->hasFlag(rme::core::TILESTATE_PROTECTIONZONE)) {
		return Fail("sample water pool should be a protection zone");
	}

	tools.setBrushKind(rme::core::BrushKind::Flags);
	tools.setFlagMask(rme::core::TILESTATE_NOLOGOUT);
	tools.beginStroke();
	tools.strokeAt(Position(102, 102, tools.floor()));
	tools.endStroke();
	if (!tools.map().getTile(Position(102, 102, tools.floor()))->hasFlag(rme::core::TILESTATE_NOLOGOUT)) {
		return Fail("flags brush should set no-logout");
	}
	tools.undo();
	if (tools.map().getTile(Position(102, 102, tools.floor()))->hasFlag(rme::core::TILESTATE_NOLOGOUT)) {
		return Fail("undo should clear the painted flag");
	}

	const uint32_t extra_town = tools.addTown("Harbor", Position(130, 130, tools.floor()));
	if (tools.map().towns().size() != 2 || extra_town == 1) {
		return Fail("addTown should append a new town id");
	}
	if (!tools.goToTown(extra_town) || tools.cameraX() != 130 || tools.cameraY() != 130) {
		return Fail("goToTown should move the camera");
	}
	tools.panBy(-5, 2);
	if (tools.cameraX() != 125 || tools.cameraY() != 132) {
		return Fail("panBy should offset the camera");
	}
	tools.addWaypoint("lookout", Position(98, 98, tools.floor()));
	if (tools.map().waypoints().size() != 2 || !tools.goToWaypoint(1) || tools.cameraX() != 98) {
		return Fail("waypoint jump failed");
	}
	if (!tools.removeTown(extra_town) || tools.map().towns().size() != 1) {
		return Fail("removeTown failed");
	}

	const auto flags_saved = (dir / "rme_phase5_flags.otbm").string();
	if (!tools.saveOtbm(flags_saved)) {
		return Fail("flag map save failed");
	}
	EditorSession flags_reload;
	if (!flags_reload.loadOtbm(flags_saved)) {
		return Fail("flag map reload failed");
	}
	const auto* reloaded_pz = flags_reload.map().getTile(Position(100, 100, flags_reload.floor()));
	if (!reloaded_pz || !reloaded_pz->hasFlag(rme::core::TILESTATE_PROTECTIONZONE)) {
		return Fail("OTBM roundtrip lost protection zone");
	}

	if (session.map().houses().size() != 1 || session.map().houses()[0].name != "Sample Cabin") {
		return Fail("sample map should load house XML");
	}
	const auto* house_tile = session.map().getTile(Position(102, 102, session.floor()));
	if (!house_tile || house_tile->getHouseID() != 1) {
		return Fail("east column should be house 1");
	}
	if (session.map().spawns().size() != 1 || session.map().spawns()[0].monsters.size() != 2) {
		return Fail("sample map should load spawn XML with two creatures");
	}
	const auto* cave = session.map().getTile(Position(100, 100, rme::MapGroundLayer + 1));
	if (!cave || !cave->hasGround() || cave->getGround()->getID() != 101) {
		return Fail("sample cave should exist on floor 8");
	}

	tools.setBrushKind(rme::core::BrushKind::House);
	tools.setHouseId(1);
	tools.beginStroke();
	tools.strokeAt(Position(101, 102, tools.floor()));
	tools.endStroke();
	if (!tools.map().getTile(Position(101, 102, tools.floor()))
		|| tools.map().getTile(Position(101, 102, tools.floor()))->getHouseID() != 1) {
		return Fail("house brush should assign house id");
	}
	tools.undo();
	if (tools.map().getTile(Position(101, 102, tools.floor()))
		&& tools.map().getTile(Position(101, 102, tools.floor()))->getHouseID() == 1) {
		return Fail("undo should clear painted house id");
	}

	const uint32_t extra_house = tools.addHouse("Annex", Position(98, 102, tools.floor()));
	if (tools.map().houses().size() != 2 || extra_house == 1) {
		return Fail("addHouse should append a new house");
	}
	if (!tools.goToHouse(extra_house) || tools.cameraX() != 98 || tools.cameraY() != 102) {
		return Fail("goToHouse should move the camera");
	}
	tools.goTo(100, 100, 8);
	if (tools.floor() != 8 || tools.cameraX() != 100) {
		return Fail("goTo should set camera and floor");
	}
	const std::size_t spawn_index = tools.addSpawn(Position(110, 110, 7), 4);
	if (tools.map().spawns().size() != 2 || !tools.goToSpawn(spawn_index) || tools.cameraX() != 110) {
		return Fail("addSpawn / goToSpawn failed");
	}
	if (!tools.addSpawnMonster(spawn_index, "Wolf", 2, 0, 120)
		|| tools.map().spawns()[spawn_index].monsters.size() != 2) {
		return Fail("addSpawnMonster failed");
	}

	const auto xml_saved = (dir / "rme_phase6_map.otbm").string();
	if (!session.saveOtbm(xml_saved)) {
		return Fail("phase 6 map save failed");
	}
	EditorSession xml_reload;
	if (!xml_reload.loadOtbm(xml_saved)) {
		return Fail("phase 6 map reload failed");
	}
	if (xml_reload.map().houses().empty() || xml_reload.map().houses()[0].name != "Sample Cabin") {
		return Fail("house XML roundtrip lost the cabin");
	}
	if (xml_reload.map().spawns().empty() || xml_reload.map().spawns()[0].monsters.size() != 2) {
		return Fail("spawn XML roundtrip lost creatures");
	}
	if (!xml_reload.map().getTile(Position(102, 99, rme::MapGroundLayer))
		|| xml_reload.map().getTile(Position(102, 99, rme::MapGroundLayer))->getHouseID() != 1) {
		return Fail("house tile OTBM roundtrip failed");
	}
	const auto* crate_reload = xml_reload.map().getTile(Position(100, 100, rme::MapGroundLayer));
	if (!crate_reload || crate_reload->getItems().size() < 2 || crate_reload->getItems().front().getActionID() != 1000
		|| crate_reload->getItems().front().getUniqueID() != 2000
		|| crate_reload->getItems().front().getText() != "Phase 8 crate"
		|| crate_reload->getItems().front().getContents().size() != 1
		|| crate_reload->getItems().front().getContents().front().getCount() != 3) {
		return Fail("OTBM roundtrip lost crate attributes or container contents");
	}
	const auto* portal_reload = xml_reload.map().getTile(Position(99, 102, rme::MapGroundLayer));
	if (!portal_reload || portal_reload->getItems().empty() || !portal_reload->getItems().front().hasDestination()
		|| portal_reload->getItems().front().getDestination().z != rme::MapGroundLayer + 1) {
		return Fail("OTBM roundtrip lost teleport destination");
	}
	const auto* door_reload = xml_reload.map().getTile(Position(100, 100, rme::MapGroundLayer + 1));
	if (!door_reload || door_reload->getItems().empty() || door_reload->getItems().front().getDoorID() != 1) {
		return Fail("OTBM roundtrip lost door id");
	}

	session.setInspect(Position(100, 100, rme::MapGroundLayer));
	rme::core::FindQuery find_box;
	find_box.item_id = 105;
	if (!session.findNext(find_box) || session.cameraX() != 100 || session.cameraY() != 100) {
		return Fail("findNext crate should jump to 100,100");
	}
	rme::core::FindQuery find_aid;
	find_aid.action_id = 7;
	if (!session.findNext(find_aid) || session.cameraX() != 99 || session.cameraY() != 102) {
		return Fail("findNext AID 7 should jump to the portal");
	}
	rme::core::FindQuery find_tp;
	find_tp.teleports_only = true;
	if (session.findTiles(find_tp).size() != 2) {
		return Fail("sample should have a cave portal and a broken teleport");
	}
	rme::core::FindQuery find_loot;
	find_loot.item_id = 104;
	bool found_crate_loot = false;
	for (const auto& pos : session.findTiles(find_loot)) {
		if (pos.x == 100 && pos.y == 100) {
			found_crate_loot = true;
		}
	}
	if (!found_crate_loot) {
		return Fail("find should see flowers inside the crate");
	}

	session.setInspect(Position(100, 100, rme::MapGroundLayer));
	if (session.browseInspect().size() != 3 || session.inspectItem() == nullptr || session.inspectItem()->getID() != 104) {
		return Fail("browse stack should default to the top cover flower");
	}
	session.setInspectIndex(1);
	if (!session.inspectItem() || session.inspectItem()->getID() != 105) {
		return Fail("inspect index 1 should be the crate");
	}
	auto props = rme::core::PropsFromItem(*session.inspectItem());
	props.action_id = 42;
	if (!session.editTopItem(props)
		|| session.map().getTile(Position(100, 100, rme::MapGroundLayer))->stackItem(1)->getActionID() != 42) {
		return Fail("editTopItem should set the selected crate action id");
	}
	session.undo();
	if (session.map().getTile(Position(100, 100, rme::MapGroundLayer))->stackItem(1)->getActionID() != 1000) {
		return Fail("undo should restore crate action id");
	}

	session.setInspectIndex(2);
	if (!session.moveInspectItem(-1)
		|| session.map().getTile(Position(100, 100, rme::MapGroundLayer))->getItems().front().getID() != 104) {
		return Fail("moveInspectItem should lower the cover flower");
	}
	session.undo();
	if (session.map().getTile(Position(100, 100, rme::MapGroundLayer))->getItems().front().getID() != 105) {
		return Fail("undo should restore overlay order");
	}

	session.setInspectIndex(1);
	if (!session.addContainerItem(103, 1)
		|| session.map().getTile(Position(100, 100, rme::MapGroundLayer))->stackItem(1)->getContents().size() != 2) {
		return Fail("addContainerItem should append loot");
	}
	if (!session.removeContainerItem(1)
		|| session.map().getTile(Position(100, 100, rme::MapGroundLayer))->stackItem(1)->getContents().size() != 1) {
		return Fail("removeContainerItem should drop the extra loot");
	}
	session.undo();
	session.undo();

	const auto issues = session.mapIssues();
	bool saw_dup = false;
	bool saw_tp = false;
	for (const auto& issue : issues) {
		if (issue.kind == rme::core::MapIssueKind::DuplicateUniqueId && issue.unique_id == 2000) {
			saw_dup = true;
		}
		if (issue.kind == rme::core::MapIssueKind::InvalidTeleport && issue.position.x == 98) {
			saw_tp = true;
		}
	}
	if (!saw_dup || !saw_tp) {
		return Fail("mapIssues should report duplicate UID 2000 and the empty teleport");
	}
	if (!session.goToIssue(0)) {
		return Fail("goToIssue should jump the camera");
	}

	std::printf("rme core test ok: %zu tiles, browse/containers, map issues, item props, find, teleports\n",
		reloaded.map().tileCount());
	return 0;
}
