#include "session.h"
#include "sprites.h"
#include "minimap.h"
#include "tile.h"
#include "zip.h"

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
	if (!session.assets().dat_loaded || session.items().size() != 33) {
		return Fail("expected 33 sample items (100-132)");
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
	if (session.sprites().count() != 33 || !session.sprites().get(1) || session.sprites().get(1)->empty()
		|| !session.sprites().get(33) || session.sprites().get(33)->empty()) {
		return Fail("sample .spr should decode sprites 1-33");
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
	if (!reloaded_items.load(dat_path, info) || reloaded_items.size() != 33) {
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

	if (session.materials().tilesets().size() != 5 || session.materials().walls().size() != 1
		|| session.materials().borders().size() != 1 || session.materials().doodads().size() != 1
		|| session.materials().doors().size() != 1 || session.materials().tables().size() != 1
		|| session.materials().carpets().size() != 1) {
		return Fail("sample materials should load tilesets, Timber, Water, Flowers, door, table, carpet");
	}
	if (!session.materials().wallForItem(106) || !session.materials().wallForItem(109)
		|| session.materials().wallForItem(103) != nullptr) {
		return Fail("timber wall set should include 106-109 but not cave wall 103");
	}
	const auto* sample_h = session.map().getTile(Position(100, 98, session.floor()));
	const auto* sample_join = session.map().getTile(Position(101, 98, session.floor()));
	const auto* sample_v = session.map().getTile(Position(101, 99, session.floor()));
	if (!sample_h || sample_h->getItems().empty() || sample_h->getItems().front().getID() != 107) {
		return Fail("sample L-wall should start with a horizontal piece at 100,98");
	}
	if (!sample_join || sample_join->getItems().empty() || sample_join->getItems().front().getID() != 109) {
		return Fail("sample L-wall corner at 101,98 should be a junction");
	}
	const auto tile_has = [](const rme::core::Tile* tile, uint16_t id) {
		if (!tile) {
			return false;
		}
		for (const auto& item : tile->getItems()) {
			if (item.getID() == id) {
				return true;
			}
		}
		return false;
	};
	if (!sample_v || !tile_has(sample_v, 108)) {
		return Fail("sample L-wall should drop a vertical piece at 101,99");
	}

	const auto* shore_nw = session.map().getTile(Position(99, 99, session.floor()));
	const auto* shore_n = session.map().getTile(Position(100, 99, session.floor()));
	const auto* shore_center = session.map().getTile(Position(100, 100, session.floor()));
	if (!tile_has(shore_nw, 117) || !tile_has(shore_n, 110) || !tile_has(sample_v, 114)) {
		return Fail("sample 3x3 water should have shore corners and a north edge");
	}
	if (tile_has(shore_center, 110) || tile_has(shore_center, 114) || tile_has(shore_center, 117)) {
		return Fail("inner water tile should not keep a shore overlay");
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

	const Position overlay_pos(98, 99, tools.floor());
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

	tools.setBrushKind(rme::core::BrushKind::Auto);
	tools.setBrushSize(1);
	tools.setBrushId(106);
	if (tools.resolvedBrush() != rme::core::BrushKind::Wall) {
		return Fail("auto brush on a timber piece should resolve to Wall");
	}

	const auto wall_id = [](const rme::core::Tile* tile) -> uint16_t {
		if (!tile || tile->getItems().empty()) {
			return 0;
		}
		return tile->getItems().back().getID();
	};

	const int z = tools.floor();
	tools.beginStroke();
	tools.strokeAt(Position(140, 140, z));
	tools.strokeAt(Position(141, 140, z));
	tools.strokeAt(Position(142, 140, z));
	tools.endStroke();
	if (wall_id(tools.map().getTile(Position(140, 140, z))) != 107
		|| wall_id(tools.map().getTile(Position(141, 140, z))) != 107
		|| wall_id(tools.map().getTile(Position(142, 140, z))) != 107) {
		return Fail("a 3-tile timber line should auto-connect as horizontal 107");
	}

	tools.beginStroke();
	tools.strokeAt(Position(141, 141, z));
	tools.endStroke();
	if (wall_id(tools.map().getTile(Position(141, 140, z))) != 109) {
		return Fail("south spur should turn the T into junction 109");
	}
	if (wall_id(tools.map().getTile(Position(141, 141, z))) != 108) {
		return Fail("south spur should be vertical 108");
	}
	if (wall_id(tools.map().getTile(Position(140, 140, z))) != 107
		|| wall_id(tools.map().getTile(Position(142, 140, z))) != 107) {
		return Fail("line ends should stay horizontal after the spur");
	}

	if (!tools.undo()) {
		return Fail("wall spur should undo");
	}
	if (wall_id(tools.map().getTile(Position(141, 140, z))) != 107
		|| wall_id(tools.map().getTile(Position(141, 141, z))) != 0) {
		return Fail("undo spur should restore the horizontal line");
	}

	tools.beginStroke();
	tools.strokeAt(Position(141, 140, z), true);
	tools.endStroke();
	if (wall_id(tools.map().getTile(Position(141, 140, z))) != 0) {
		return Fail("shift+wall should remove the timber family");
	}
	if (wall_id(tools.map().getTile(Position(140, 140, z))) != 106
		|| wall_id(tools.map().getTile(Position(142, 140, z))) != 106) {
		return Fail("removing the middle of a line should restitch ends to poles");
	}

	if (!tools.undo() || wall_id(tools.map().getTile(Position(141, 140, z))) != 107) {
		return Fail("undo wall erase should restore the line");
	}

	tools.setBrushKind(rme::core::BrushKind::Auto);
	tools.setBrushSize(1);
	tools.setBrushId(102);
	if (tools.resolvedBrush() != rme::core::BrushKind::Border) {
		return Fail("auto brush on water should resolve to Border");
	}

	const auto shore_id = [](const rme::core::Tile* tile) -> uint16_t {
		if (!tile) {
			return 0;
		}
		for (const auto& item : tile->getItems()) {
			if (item.getID() >= 110 && item.getID() <= 117) {
				return item.getID();
			}
		}
		return 0;
	};

	tools.beginStroke();
	tools.strokeAt(Position(150, 150, z));
	tools.strokeAt(Position(151, 150, z));
	tools.strokeAt(Position(150, 151, z));
	tools.strokeAt(Position(151, 151, z));
	tools.endStroke();
	if (shore_id(tools.map().getTile(Position(150, 150, z))) != 117
		|| shore_id(tools.map().getTile(Position(151, 150, z))) != 114
		|| shore_id(tools.map().getTile(Position(150, 151, z))) != 116
		|| shore_id(tools.map().getTile(Position(151, 151, z))) != 115) {
		return Fail("a 2x2 water pond should get four outer-corner shores");
	}

	tools.beginStroke();
	tools.strokeAt(Position(152, 150, z));
	tools.endStroke();
	if (shore_id(tools.map().getTile(Position(151, 150, z))) != 110) {
		return Fail("extending the north shore should restitch the middle to edge 110");
	}
	if (shore_id(tools.map().getTile(Position(152, 150, z))) != 114) {
		return Fail("new north-east water should be a NE shore");
	}

	if (!tools.undo() || shore_id(tools.map().getTile(Position(151, 150, z))) != 114
		|| tools.map().getTile(Position(152, 150, z))) {
		return Fail("undo border stroke should restore the 2x2 pond");
	}

	tools.beginStroke();
	tools.strokeAt(Position(151, 150, z), true);
	tools.endStroke();
	if (tools.map().getTile(Position(151, 150, z))
		&& tools.map().getTile(Position(151, 150, z))->hasGround()
		&& tools.map().getTile(Position(151, 150, z))->getGround()->getID() == 102) {
		return Fail("shift+border should remove the water ground");
	}
	if (shore_id(tools.map().getTile(Position(150, 150, z))) != 114) {
		return Fail("removing the NE pond tile should restitch 150,150 to a NE shore");
	}

	if (!session.materials().doodadForItem(104) || session.materials().doodadForItem(100) != nullptr) {
		return Fail("flower 104 should be a doodad; grass should not");
	}
	if (!rme::core::DoodadHits(Position(0, 0, 7), 100) || rme::core::DoodadHits(Position(0, 0, 7), 0)) {
		return Fail("doodad chance 100 always hits and 0 never hits");
	}

	tools.setBrushKind(rme::core::BrushKind::Auto);
	tools.setBrushSize(1);
	tools.setBrushId(104);
	if (tools.resolvedBrush() != rme::core::BrushKind::Doodad) {
		return Fail("auto brush on a flower should resolve to Doodad");
	}

	Position doodad_hit(160, 160, z);
	Position doodad_miss(160, 160, z);
	bool found_hit = false;
	bool found_miss = false;
	for (int y = 160; y < 180 && (!found_hit || !found_miss); ++y) {
		for (int x = 160; x < 180 && (!found_hit || !found_miss); ++x) {
			const Position pos(x, y, z);
			if (!found_hit && rme::core::DoodadHits(pos, 60)) {
				doodad_hit = pos;
				found_hit = true;
			} else if (!found_miss && !rme::core::DoodadHits(pos, 60)) {
				doodad_miss = pos;
				found_miss = true;
			}
		}
	}
	if (!found_hit || !found_miss) {
		return Fail("expected both doodad hit and miss tiles in 160-179");
	}

	tools.beginStroke();
	tools.strokeAt(doodad_hit);
	tools.strokeAt(doodad_miss);
	tools.endStroke();
	if (!tile_has(tools.map().getTile(doodad_hit), 104)) {
		return Fail("doodad hit tile should receive a flower");
	}
	if (tile_has(tools.map().getTile(doodad_miss), 104)) {
		return Fail("doodad miss tile should stay empty at 60% chance");
	}

	tools.setBrushSize(3);
	const Position scatter(170, 170, z);
	int expected_scatter = 0;
	for (const auto& cell : tools.hoverFootprint(scatter)) {
		if (rme::core::DoodadHits(cell, 60)) {
			++expected_scatter;
		}
	}
	tools.beginStroke();
	tools.strokeAt(scatter);
	tools.endStroke();
	int got_scatter = 0;
	for (const auto& cell : tools.hoverFootprint(scatter)) {
		if (tile_has(tools.map().getTile(cell), 104)) {
			++got_scatter;
		}
	}
	if (got_scatter != expected_scatter) {
		return Fail("3x3 doodad scatter should match DoodadHits");
	}

	tools.setBrushSize(1);
	tools.beginStroke();
	tools.strokeAt(doodad_hit, true);
	tools.endStroke();
	if (tile_has(tools.map().getTile(doodad_hit), 104)) {
		return Fail("shift+doodad should remove the flower family");
	}
	if (!tools.undo() || !tile_has(tools.map().getTile(doodad_hit), 104)) {
		return Fail("undo doodad erase should restore the flower");
	}

	tools.setBrushKind(rme::core::BrushKind::Auto);
	tools.setBrushSize(1);
	tools.setBrushId(118);
	if (tools.resolvedBrush() != rme::core::BrushKind::Door) {
		return Fail("auto brush on a door should resolve to Door");
	}
	tools.setBrushId(106);
	tools.beginStroke();
	tools.strokeAt(Position(180, 150, z));
	tools.strokeAt(Position(181, 150, z));
	tools.endStroke();
	tools.setBrushId(118);
	tools.beginStroke();
	tools.strokeAt(Position(181, 150, z));
	tools.endStroke();
	if (!tile_has(tools.map().getTile(Position(181, 150, z)), 118)) {
		return Fail("door on a horizontal wall should pick the horizontal door");
	}

	tools.setBrushId(120);
	if (tools.resolvedBrush() != rme::core::BrushKind::Table) {
		return Fail("auto brush on a table should resolve to Table");
	}
	tools.beginStroke();
	tools.strokeAt(Position(190, 150, z));
	tools.strokeAt(Position(191, 150, z));
	tools.endStroke();
	if (!tile_has(tools.map().getTile(Position(190, 150, z)), 121)
		|| !tile_has(tools.map().getTile(Position(191, 150, z)), 121)) {
		return Fail("two adjacent tables should restitch to horizontal pieces");
	}

	tools.setBrushId(124);
	if (tools.resolvedBrush() != rme::core::BrushKind::Carpet) {
		return Fail("auto brush on a carpet should resolve to Carpet");
	}
	tools.beginStroke();
	tools.strokeAt(Position(200, 150, z));
	tools.strokeAt(Position(201, 150, z));
	tools.strokeAt(Position(200, 151, z));
	tools.strokeAt(Position(201, 151, z));
	tools.endStroke();
	if (!tile_has(tools.map().getTile(Position(200, 150, z)), 132)
		|| !tile_has(tools.map().getTile(Position(201, 150, z)), 129)
		|| !tile_has(tools.map().getTile(Position(200, 151, z)), 131)
		|| !tile_has(tools.map().getTile(Position(201, 151, z)), 130)) {
		return Fail("2x2 carpet should restitch to outer corners");
	}
	tools.beginStroke();
	tools.strokeAt(Position(200, 150, z), true);
	tools.endStroke();
	if (tile_has(tools.map().getTile(Position(200, 150, z)), 132)
		|| tile_has(tools.map().getTile(Position(200, 150, z)), 124)) {
		return Fail("shift+carpet should remove that carpet tile");
	}
	if (!tools.undo() || !tile_has(tools.map().getTile(Position(200, 150, z)), 132)) {
		return Fail("undo carpet erase should restore the NW corner");
	}

	EditorSession blank;
	blank.newMap(512, 256, "Town Square");
	if (blank.map().getWidth() != 512 || blank.map().getHeight() != 256 || blank.map().tileCount() != 0
		|| blank.otbmFileName() != "Town Square.otbm" || blank.zipFileName() != "Town Square.zip") {
		return Fail("newMap should set size and a sanitized file name on an empty map");
	}
	blank.newMap(8, 8, "");
	if (blank.map().getWidth() != 256 || blank.map().getHeight() != 256 || blank.otbmFileName() != "Untitled.otbm") {
		return Fail("newMap should clamp below 256 and default the name");
	}

	session.setMapName("harbor");
	session.setMapDescription("Phase 13 harbor");
	if (!session.map().hasChanged()) {
		return Fail("map properties should mark the map dirty");
	}
	const auto named = (dir / "harbor.otbm").string();
	if (!session.saveOtbm(named)) {
		return Fail("save after renaming failed");
	}
	if (session.map().hasChanged()) {
		return Fail("save should clear the dirty flag");
	}
	EditorSession named_reload;
	if (!named_reload.loadOtbm(named) || named_reload.map().getDescription() != "Phase 13 harbor"
		|| named_reload.otbmFileName() != "harbor.otbm") {
		return Fail("OTBM should roundtrip map name and description");
	}

	const auto zip_path = (dir / "harbor.zip").string();
	if (!session.saveMapZip(zip_path)) {
		return Fail("saveMapZip failed");
	}
	std::vector<rme::core::ZipEntry> zipped;
	if (!rme::core::ReadStoreZip(zip_path, zipped) || zipped.size() != 3) {
		return Fail("zip should contain otbm + houses.xml + spawn.xml");
	}
	bool saw_otbm = false;
	bool saw_houses = false;
	bool saw_spawns = false;
	const auto extracted = (dir / "from_zip.otbm").string();
	for (const auto& entry : zipped) {
		if (entry.name == "harbor.otbm") {
			saw_otbm = true;
			FILE* out = std::fopen(extracted.c_str(), "wb");
			if (!out || std::fwrite(entry.data.data(), 1, entry.data.size(), out) != entry.data.size()) {
				if (out) {
					std::fclose(out);
				}
				return Fail("could not extract otbm from zip");
			}
			std::fclose(out);
		} else if (entry.name == "houses.xml") {
			saw_houses = true;
		} else if (entry.name == "spawn.xml") {
			saw_spawns = true;
		}
	}
	if (!saw_otbm || !saw_houses || !saw_spawns) {
		return Fail("zip entry names should match the map bundle");
	}
	EditorSession from_zip;
	if (!from_zip.loadOtbm(extracted) || from_zip.map().getDescription() != "Phase 13 harbor") {
		return Fail("otbm inside the zip should load with the saved description");
	}
	if (rme::core::MapOtbmFileName("../bad:.zip") != "bad_.otbm") {
		return Fail("map file names should drop paths and illegal characters");
	}

	tools.setBrushKind(rme::core::BrushKind::Creature);
	tools.setCreatureName("Wolf");
	tools.setSpawnRadius(3);
	tools.setSpawnTime(90);
	const std::size_t before_spawns = tools.map().spawns().size();
	tools.beginStroke();
	tools.strokeAt(Position(180, 160, z));
	tools.strokeAt(Position(181, 160, z));
	tools.endStroke();
	if (tools.map().spawns().size() != before_spawns + 1) {
		return Fail("creature stroke should create one spawn for nearby tiles");
	}
	const auto& pack = tools.map().spawns().back();
	if (pack.center != Position(180, 160, z) || pack.radius != 3 || pack.monsters.size() != 2) {
		return Fail("first painted tile should be the spawn center with two wolves");
	}
	if (pack.monsters[0].name != "Wolf" || pack.monsters[1].name != "Wolf" || pack.monsters[1].dx != 1
		|| pack.monsters[1].spawntime != 90) {
		return Fail("adjacent creature should share the spawn with dx=1");
	}
	if (tools.creaturesAt(Position(180, 160, z)).size() != 1
		|| tools.creaturesAt(Position(180, 160, z)).front() != "Wolf") {
		return Fail("creaturesAt should find the painted wolf");
	}

	tools.setCreatureName("Orc");
	tools.beginStroke();
	tools.strokeAt(Position(180, 160, z));
	tools.endStroke();
	if (tools.creaturesAt(Position(180, 160, z)).front() != "Orc") {
		return Fail("painting over a creature should replace the name");
	}

	tools.beginStroke();
	tools.strokeAt(Position(200, 200, z));
	tools.endStroke();
	if (tools.map().spawns().size() != before_spawns + 2) {
		return Fail("a tile outside the radius should start a new spawn");
	}

	tools.setBrushKind(rme::core::BrushKind::Ground);
	tools.setBrushId(100);
	tools.beginStroke();
	tools.strokeAt(Position(182, 160, z));
	tools.endStroke();
	tools.setBrushKind(rme::core::BrushKind::Creature);
	tools.setCreatureName("Snake");
	tools.beginStroke();
	tools.strokeAt(Position(182, 160, z));
	tools.endStroke();
	tools.beginStroke();
	tools.strokeAt(Position(182, 160, z), true);
	tools.endStroke();
	if (!tools.creaturesAt(Position(182, 160, z)).empty()) {
		return Fail("shift+creature should remove the monster");
	}
	const auto* grass_tile = tools.map().getTile(Position(182, 160, z));
	if (!grass_tile || !grass_tile->getGround() || grass_tile->getGround()->getID() != 100) {
		return Fail("shift+creature must not erase ground");
	}

	const auto creature_saved = (dir / "rme_phase14_creatures.otbm").string();
	if (!tools.saveOtbm(creature_saved)) {
		return Fail("creature map save failed");
	}
	EditorSession creature_reload;
	if (!creature_reload.loadOtbm(creature_saved) || creature_reload.creaturesAt(Position(181, 160, z)).empty()
		|| creature_reload.creaturesAt(Position(181, 160, z)).front() != "Wolf") {
		return Fail("spawn XML should keep painted creatures");
	}

	tools.setBrushKind(rme::core::BrushKind::Ground);
	tools.setBrushSize(1);
	tools.setBrushId(100);
	tools.beginStroke();
	tools.strokeAt(Position(160, 180, z));
	tools.strokeAt(Position(161, 180, z));
	tools.endStroke();
	tools.setBrushId(101);
	tools.beginStroke();
	tools.strokeAt(Position(160, 181, z));
	tools.endStroke();
	tools.selection().begin(Position(160, 180, z));
	tools.selection().update(Position(161, 181, z));
	tools.selection().finish();
	if (!tools.rotateSelection(true)) {
		return Fail("rotate CW failed");
	}
	const auto ground_id = [&](int x, int y) -> int {
		const auto* tile = tools.map().getTile(Position(x, y, z));
		if (!tile || !tile->getGround()) {
			return 0;
		}
		return tile->getGround()->getID();
	};
	if (ground_id(161, 180) != 100 || ground_id(161, 181) != 100 || ground_id(160, 180) != 101) {
		return Fail("CW rotate should turn the L");
	}
	if (ground_id(160, 181) != 0) {
		return Fail("CW rotate should leave the old dirt cell empty");
	}
	if (!tools.undo()) {
		return Fail("undo rotate failed");
	}
	if (ground_id(160, 180) != 100 || ground_id(161, 180) != 100 || ground_id(160, 181) != 101) {
		return Fail("undo should restore the L");
	}
	tools.selection().begin(Position(160, 180, z));
	tools.selection().update(Position(161, 181, z));
	tools.selection().finish();
	if (!tools.rotateSelection(true) || !tools.flipSelection(true)) {
		return Fail("flip H failed");
	}
	if (ground_id(160, 180) != 100 || ground_id(161, 180) != 101 || ground_id(160, 181) != 100) {
		return Fail("flip H should mirror the rotated L");
	}

	tools.selection().begin(Position(160, 180, z));
	tools.selection().update(Position(161, 181, z));
	tools.selection().finish();
	if (tools.replaceItems(100, 102, true) != 2) {
		return Fail("selection replace should change two grass tiles");
	}
	if (ground_id(160, 180) != 102 || ground_id(160, 181) != 102 || ground_id(161, 180) != 101) {
		return Fail("replace should only change id 100");
	}
	if (tools.replaceItems(104, 105, false) == 0) {
		return Fail("map replace should find flowers");
	}
	const auto* crate_tile = tools.map().getTile(Position(100, 100, z));
	if (!crate_tile || crate_tile->getItems().empty() || crate_tile->getItems().front().getContents().empty()
		|| crate_tile->getItems().front().getContents().front().getID() != 105) {
		return Fail("replace should walk container contents");
	}

	const auto all_items = tools.paletteItems("", -1);
	if (all_items.size() != 33) {
		return Fail("empty palette query should list every sample item");
	}
	const auto grass_hits = tools.paletteItems("grass", -1);
	if (grass_hits.size() != 1 || grass_hits.front() != 100) {
		return Fail("palette search grass should find item 100");
	}
	const auto wall_hits = tools.paletteItems("WALL", -1);
	if (wall_hits.size() != 5) {
		return Fail("palette search wall should find the timber set");
	}
	const auto id_hits = tools.paletteItems(" 100 ", -1);
	if (id_hits.size() != 1 || id_hits.front() != 100) {
		return Fail("palette search should match decimal ids");
	}
	const auto grounds_dirt = tools.paletteItems("dirt", 0);
	if (grounds_dirt.size() != 1 || grounds_dirt.front() != 101) {
		return Fail("palette search should stay inside the selected tileset");
	}
	if (!tools.paletteItems("wall", 0).empty()) {
		return Fail("grounds tileset should not contain walls");
	}

	const auto client_dir = (dir / "rme_phase17_client").string();
	std::filesystem::create_directories(client_dir);
	const auto client_dat = (std::filesystem::path(client_dir) / "Tibia.dat").string();
	const auto client_spr = (std::filesystem::path(client_dir) / "Tibia.spr").string();
	if (!tools.createSampleAssets(client_dat, client_spr)) {
		return Fail("phase 17 sample client write failed");
	}
	{
		const auto decoy = std::filesystem::path(client_dir) / "other.dat";
		std::FILE* out = std::fopen(decoy.string().c_str(), "wb");
		if (!out) {
			return Fail("could not write decoy dat");
		}
		const char junk[] = "not a dat";
		std::fwrite(junk, 1, sizeof(junk) - 1, out);
		std::fclose(out);
	}
	EditorSession restored_client;
	if (!restored_client.loadClientDirectory(client_dir) || restored_client.items().size() != 33
		|| !restored_client.assets().spr_loaded) {
		return Fail("loadClientDirectory should prefer Tibia.dat / Tibia.spr");
	}
	if (restored_client.assets().dat_path.find("Tibia.dat") == std::string::npos) {
		return Fail("restored dat path should be Tibia.dat");
	}
	EditorSession empty_client;
	if (empty_client.loadClientDirectory((dir / "rme_phase17_empty").string())) {
		return Fail("empty client directory should not load");
	}

	const auto last_dir = (dir / "rme_phase18_map").string();
	std::filesystem::create_directories(last_dir);
	EditorSession last_map;
	if (!last_map.createSampleMap((std::filesystem::path(last_dir) / "seed.otbm").string())) {
		return Fail("phase 18 seed map failed");
	}
	last_map.setMapName("phase18");
	last_map.setMapDescription("Phase 18 last map");
	const auto last_zip = (std::filesystem::path(last_dir) / "phase18.zip").string();
	if (!last_map.saveMapZip(last_zip) || !last_map.rememberLastMap(last_dir, "phase18.zip")) {
		return Fail("remember last map zip failed");
	}
	if (!last_map.rememberLastMap(last_dir, "../phase18.zip")) {
		return Fail("rememberLastMap should keep only the file name");
	}
	EditorSession last_reload;
	if (!last_reload.loadLastMap(last_dir) || last_reload.map().getDescription() != "Phase 18 last map"
		|| last_reload.otbmFileName() != "phase18.otbm") {
		return Fail("loadLastMap should restore the zip pointer");
	}
	if (!last_reload.forgetLastMap(last_dir) || last_reload.loadLastMap(last_dir)) {
		return Fail("forgetLastMap should drop the pointer");
	}
	if (!last_reload.rememberLastMap(last_dir, "phase18.otbm") || !last_reload.loadLastMap(last_dir)
		|| last_reload.map().getDescription() != "Phase 18 last map") {
		return Fail("loadLastMap should restore a plain OTBM");
	}
	if (last_reload.loadLastMap((dir / "rme_phase18_empty").string())) {
		return Fail("empty last-map folder should not load");
	}

	std::printf("rme core test ok: %zu tiles, last map, persist client dir, palette search\n",
		reloaded.map().tileCount());
	return 0;
}
