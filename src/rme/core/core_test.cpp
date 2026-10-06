#include "session.h"
#include "sprites.h"

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
	if (session.map().tileCount() != 25) {
		std::fprintf(stderr, "expected 25 sample tiles, got %zu\n", session.map().tileCount());
		return 1;
	}
	const auto* center = session.map().getTile(Position(100, 100, session.floor()));
	if (!center || !center->getGround() || center->getItems().size() != 1 || center->getItems().front().getID() != 105) {
		return Fail("sample map center should have a box overlay");
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

	std::printf("rme core test ok: %zu tiles, 6 dat items, 6 sprites, brushes/fill/selection, item 351 persisted\n",
		reloaded.map().tileCount());
	return 0;
}
