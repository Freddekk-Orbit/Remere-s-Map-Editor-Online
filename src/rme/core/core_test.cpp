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

	std::printf("rme core test ok: %zu tiles, 6 dat items, 6 sprites, item 351 persisted, undo/redo works\n",
		reloaded.map().tileCount());
	return 0;
}
