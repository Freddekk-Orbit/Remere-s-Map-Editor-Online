#include "session.h"
#include "map_xml.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <sstream>
#include <unordered_map>

namespace rme {
namespace core {

EditorSession::EditorSession() {
	newMap();
}

void EditorSession::newMap(int width, int height) {
	cancelStroke();
	selection_.clear();
	map_.createEmpty(width, height);
	history_.clear();
	markMinimapDirty();
	last_error_.clear();
	camera_x_ = width / 2;
	camera_y_ = height / 2;
	floor_ = rme::MapGroundLayer;
	inspect_ = Position(camera_x_, camera_y_, floor_);
	inspect_index_ = -1;
}

bool EditorSession::loadOtbm(const std::string& path) {
	cancelStroke();
	selection_.clear();
	Map loaded;
	if (!LoadOTBM(loaded, path)) {
		last_error_ = loaded.getError().empty() ? "Failed to load OTBM" : loaded.getError();
		return false;
	}
	map_ = std::move(loaded);
	history_.clear();
	markMinimapDirty();
	centerOnOccupied();
	inspect_ = Position(camera_x_, camera_y_, floor_);
	inspect_index_ = -1;
	LoadHouseXml(map_, CompanionPath(path, map_.getHouseFilename(), "houses.xml"));
	LoadSpawnXml(map_, CompanionPath(path, map_.getSpawnFilename(), "spawn.xml"));
	if (!map_.houses().empty()) {
		house_id_ = map_.houses().front().id;
	}
	last_error_.clear();
	return true;
}

bool EditorSession::saveOtbm(const std::string& path) {
	if (map_.getHouseFilename().empty()) {
		map_.setHouseFilename("houses.xml");
	}
	if (map_.getSpawnFilename().empty()) {
		map_.setSpawnFilename("spawn.xml");
	}
	if (!SaveOTBM(map_, path)) {
		last_error_ = "Failed to write " + path;
		return false;
	}
	const std::string houses = CompanionPath(path, map_.getHouseFilename(), "houses.xml");
	const std::string spawns = CompanionPath(path, map_.getSpawnFilename(), "spawn.xml");
	if (!SaveHouseXml(map_, houses) || !SaveSpawnXml(map_, spawns)) {
		last_error_ = "Wrote OTBM but failed to write house/spawn XML";
		return false;
	}
	map_.setName(std::filesystem::path(path).filename().string());
	map_.clearChanges();
	last_error_.clear();
	return true;
}

bool EditorSession::loadDat(const std::string& path) {
	if (!items_.load(path, assets_)) {
		last_error_ = assets_.error.empty() ? "Failed to read .dat" : assets_.error;
		return false;
	}
	const auto materials_path = std::filesystem::path(path).parent_path() / "materials.xml";
	if (!materials_.load(materials_path.string())) {
		materials_.ensureDefaults();
	}
	last_error_.clear();
	return true;
}

bool EditorSession::loadMaterials(const std::string& path) {
	if (!materials_.load(path)) {
		last_error_ = materials_.lastError().empty() ? "Failed to read materials.xml" : materials_.lastError();
		return false;
	}
	last_error_.clear();
	return true;
}

bool EditorSession::loadSpr(const std::string& path) {
	std::string error;
	uint32_t signature = 0;
	uint32_t count = 0;
	if (!sprites_.load(path, signature, count, error)) {
		assets_.spr_loaded = false;
		assets_.error = error;
		last_error_ = error.empty() ? "Failed to read .spr" : error;
		return false;
	}
	assets_.spr_loaded = true;
	assets_.spr_signature = signature;
	assets_.sprite_count = count;
	assets_.spr_path = path;
	assets_.error.clear();
	last_error_.clear();
	return true;
}

bool EditorSession::loadHouseXml(const std::string& path) {
	if (!LoadHouseXml(map_, path)) {
		last_error_ = "Failed to read " + path;
		return false;
	}
	if (!map_.houses().empty()) {
		house_id_ = map_.houses().front().id;
	}
	last_error_.clear();
	return true;
}

bool EditorSession::loadSpawnXml(const std::string& path) {
	if (!LoadSpawnXml(map_, path)) {
		last_error_ = "Failed to read " + path;
		return false;
	}
	last_error_.clear();
	return true;
}

bool EditorSession::createSampleMap(const std::string& path) {
	if (!WriteSampleOTBM(path)) {
		last_error_ = "Could not write sample OTBM";
		return false;
	}
	return loadOtbm(path);
}

bool EditorSession::createSampleAssets(const std::string& dat_path, const std::string& spr_path) {
	if (!items_.writeSample(dat_path)) {
		last_error_ = "Could not write sample Tibia.dat";
		return false;
	}
	if (!sprites_.writeSample(spr_path)) {
		last_error_ = "Could not write sample Tibia.spr";
		return false;
	}
	const auto materials_path = (std::filesystem::path(dat_path).parent_path() / "materials.xml").string();
	if (!materials_.writeSample(materials_path)) {
		last_error_ = materials_.lastError().empty() ? "Could not write materials.xml" : materials_.lastError();
		return false;
	}
	if (!loadDat(dat_path) || !loadSpr(spr_path)) {
		return false;
	}
	last_error_.clear();
	return true;
}

void EditorSession::paintGround(const Position& position, uint16_t item_id) {
	endStroke();
	Action action(ActionIdentifier::Draw);
	TileChange change;
	change.position = position;
	if (const Tile* existing = map_.getTile(position)) {
		change.before = existing->deepCopy();
	} else {
		change.before.setPosition(position);
	}
	change.after = change.before.deepCopy();
	change.after.setPosition(position);
	change.after.setGround(Item(item_id));
	action.addChange(std::move(change));
	history_.add(std::move(action), map_);
}

void EditorSession::eraseTile(const Position& position) {
	endStroke();
	const Tile* existing = map_.getTile(position);
	if (!existing) {
		return;
	}
	Action action(ActionIdentifier::Erase);
	TileChange change;
	change.position = position;
	change.before = existing->deepCopy();
	change.after.setPosition(position);
	action.addChange(std::move(change));
	history_.add(std::move(action), map_);
}

BrushKind EditorSession::resolvedBrush() const {
	if (brush_kind_ == BrushKind::Auto && materials_.borderForItem(brush_id_)) {
		return BrushKind::Border;
	}
	if (brush_kind_ == BrushKind::Auto && materials_.wallForItem(brush_id_)) {
		return BrushKind::Wall;
	}
	if (brush_kind_ == BrushKind::Auto && materials_.doodadForItem(brush_id_)) {
		return BrushKind::Doodad;
	}
	if (brush_kind_ == BrushKind::Auto && materials_.doorForItem(brush_id_)) {
		return BrushKind::Door;
	}
	if (brush_kind_ == BrushKind::Auto && materials_.tableForItem(brush_id_)) {
		return BrushKind::Table;
	}
	if (brush_kind_ == BrushKind::Auto && materials_.carpetForItem(brush_id_)) {
		return BrushKind::Carpet;
	}
	return ResolveBrush(brush_kind_, items_.get(brush_id_));
}

void EditorSession::setBrushId(uint16_t id) {
	brush_id_ = id;
}

void EditorSession::setBrushSize(int size) {
	size = std::clamp(size, 1, 9);
	if (size % 2 == 0) {
		++size;
	}
	brush_size_ = size;
}

std::vector<Position> EditorSession::hoverFootprint(const Position& center) const {
	return BrushFootprint(center, brush_size_, map_.getWidth(), map_.getHeight());
}

void EditorSession::cancelStroke() {
	stroking_ = false;
	stroke_ = Action(ActionIdentifier::BrushStroke);
	stroke_seen_.clear();
}

void EditorSession::beginStroke() {
	if (stroking_) {
		endStroke();
	}
	stroking_ = true;
	stroke_ = Action(ActionIdentifier::BrushStroke);
	stroke_seen_.clear();
}

bool EditorSession::applyLive(const Position& position, BrushKind kind, uint16_t item_id, Action& action, bool invert) {
	if (!map_.inBounds(position)) {
		return false;
	}
	TileChange change;
	change.position = position;
	if (const Tile* existing = map_.getTile(position)) {
		change.before = existing->deepCopy();
	} else {
		change.before.setPosition(position);
	}
	change.after = change.before.deepCopy();
	change.after.setPosition(position);
	if (!ApplyBrushToTile(change.after, kind, item_id, flag_mask_, invert && (kind == BrushKind::Flags || kind == BrushKind::House), house_id_)) {
		return false;
	}
	if (change.after.empty()) {
		map_.removeTile(position);
	} else {
		map_.setTile(change.after.deepCopy());
	}
	action.addChange(std::move(change));
	markMinimapDirty();
	return true;
}

namespace {

bool TileLooksEqual(const Tile& a, const Tile& b) {
	if (a.getHouseID() != b.getHouseID() || a.getFlags() != b.getFlags()) {
		return false;
	}
	if (a.hasGround() != b.hasGround()) {
		return false;
	}
	if (a.hasGround() && a.getGround()->getID() != b.getGround()->getID()) {
		return false;
	}
	if (a.getItems().size() != b.getItems().size()) {
		return false;
	}
	for (std::size_t i = 0; i < a.getItems().size(); ++i) {
		if (a.getItems()[i].getID() != b.getItems()[i].getID()) {
			return false;
		}
	}
	return true;
}

} // namespace

bool EditorSession::writeTileAfter(Action& action, const Position& position, Tile after) {
	after.setPosition(position);
	for (auto it = action.changes().begin(); it != action.changes().end(); ++it) {
		if (!(it->position == position)) {
			continue;
		}
		it->after = after.deepCopy();
		if (TileLooksEqual(it->before, it->after)) {
			if (it->before.empty()) {
				map_.removeTile(position);
			} else {
				map_.setTile(it->before.deepCopy());
			}
			action.changes().erase(it);
			markMinimapDirty();
			return false;
		}
		if (after.empty()) {
			map_.removeTile(position);
		} else {
			map_.setTile(after.deepCopy());
		}
		markMinimapDirty();
		return true;
	}

	TileChange change;
	change.position = position;
	if (const Tile* existing = map_.getTile(position)) {
		change.before = existing->deepCopy();
	} else {
		change.before.setPosition(position);
	}
	change.after = after.deepCopy();
	if (TileLooksEqual(change.before, change.after)) {
		return false;
	}
	if (after.empty()) {
		map_.removeTile(position);
	} else {
		map_.setTile(after.deepCopy());
	}
	action.addChange(std::move(change));
	markMinimapDirty();
	return true;
}

void EditorSession::strokeWallAt(const Position& position, bool invert) {
	const WallSet* set = materials_.wallForItem(brush_id_);
	if (!set) {
		last_error_ = "Wall brush needs a wall item from materials.xml";
		return;
	}
	last_error_.clear();

	std::vector<Position> seeds;
	for (const Position& cell : BrushFootprint(position, brush_size_, map_.getWidth(), map_.getHeight())) {
		seeds.push_back(cell);
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (stroke_seen_.count(key)) {
			continue;
		}
		stroke_seen_.insert(key);

		Tile after;
		if (const Tile* existing = map_.getTile(cell)) {
			after = existing->deepCopy();
		} else {
			after.setPosition(cell);
		}
		after.setPosition(cell);

		bool changed = false;
		if (invert) {
			changed = RemoveWallFromTile(after, *set);
		} else if (!TileHasWall(after, *set)) {
			changed = ApplyWallToTile(after, set->pieceFor(0), *set);
		}
		if (changed) {
			writeTileAfter(stroke_, cell, std::move(after));
		}
	}

	auto restitchCell = [&](const Position& cell) {
		if (!map_.inBounds(cell)) {
			return;
		}
		const Tile* tile = map_.getTile(cell);
		if (!tile || !TileHasWall(*tile, *set)) {
			return;
		}
		Tile after = tile->deepCopy();
		const uint16_t piece = ResolveWallPiece(map_, cell, *set);
		if (ApplyWallToTile(after, piece, *set)) {
			writeTileAfter(stroke_, cell, std::move(after));
		}
	};

	constexpr int kDx[4] = {0, 1, 0, -1};
	constexpr int kDy[4] = {-1, 0, 1, 0};
	std::unordered_set<uint64_t> restitched;
	for (const Position& cell : seeds) {
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (restitched.insert(key).second) {
			restitchCell(cell);
		}
		for (int i = 0; i < 4; ++i) {
			const Position neighbor(cell.x + kDx[i], cell.y + kDy[i], cell.z);
			const uint64_t nkey = MakeTileKey(neighbor.x, neighbor.y, neighbor.z);
			if (restitched.insert(nkey).second) {
				restitchCell(neighbor);
			}
		}
	}
}

void EditorSession::restitchBorders(Action& action, const GroundBorderSet& set, const std::vector<Position>& seeds) {
	constexpr int kDx[4] = {0, 1, 0, -1};
	constexpr int kDy[4] = {-1, 0, 1, 0};
	std::unordered_set<uint64_t> seen;
	auto restitchCell = [&](const Position& cell) {
		if (!map_.inBounds(cell)) {
			return;
		}
		const Tile* tile = map_.getTile(cell);
		if (!tile) {
			return;
		}
		Tile after = tile->deepCopy();
		if (ApplyBordersToTile(after, map_, set)) {
			writeTileAfter(action, cell, std::move(after));
		}
	};
	for (const Position& cell : seeds) {
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (seen.insert(key).second) {
			restitchCell(cell);
		}
		for (int i = 0; i < 4; ++i) {
			const Position neighbor(cell.x + kDx[i], cell.y + kDy[i], cell.z);
			const uint64_t nkey = MakeTileKey(neighbor.x, neighbor.y, neighbor.z);
			if (seen.insert(nkey).second) {
				restitchCell(neighbor);
			}
		}
	}
}

void EditorSession::strokeBorderAt(const Position& position, bool invert) {
	const GroundBorderSet* set = materials_.borderForItem(brush_id_);
	if (!set) {
		last_error_ = "Border brush needs a ground from materials.xml";
		return;
	}
	last_error_.clear();

	std::vector<Position> seeds;
	for (const Position& cell : BrushFootprint(position, brush_size_, map_.getWidth(), map_.getHeight())) {
		seeds.push_back(cell);
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (stroke_seen_.count(key)) {
			continue;
		}
		stroke_seen_.insert(key);

		Tile after;
		if (const Tile* existing = map_.getTile(cell)) {
			after = existing->deepCopy();
		} else {
			after.setPosition(cell);
		}
		after.setPosition(cell);

		bool changed = false;
		if (invert) {
			changed = RemoveBordersFromTile(after, *set);
			if (TileHasInnerGround(after, *set)) {
				after.clearGround();
				changed = true;
			}
		} else if (!TileHasInnerGround(after, *set)) {
			after.setGround(Item(set->inner_id));
			changed = true;
		}
		if (changed) {
			writeTileAfter(stroke_, cell, std::move(after));
		}
	}
	restitchBorders(stroke_, *set, seeds);
}

void EditorSession::strokeDoodadAt(const Position& position, bool invert) {
	const DoodadSet* set = materials_.doodadForItem(brush_id_);
	if (!set) {
		last_error_ = "Doodad brush needs a doodad item from materials.xml";
		return;
	}
	last_error_.clear();

	for (const Position& cell : BrushFootprint(position, brush_size_, map_.getWidth(), map_.getHeight())) {
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (stroke_seen_.count(key)) {
			continue;
		}
		stroke_seen_.insert(key);
		if (!invert && !DoodadHits(cell, set->chance)) {
			continue;
		}

		Tile after;
		if (const Tile* existing = map_.getTile(cell)) {
			after = existing->deepCopy();
		} else {
			after.setPosition(cell);
		}
		after.setPosition(cell);

		bool changed = false;
		if (invert) {
			changed = RemoveDoodadFromTile(after, *set);
		} else {
			changed = ApplyDoodadToTile(after, set->pick(cell));
		}
		if (changed) {
			writeTileAfter(stroke_, cell, std::move(after));
		}
	}
}

void EditorSession::strokeDoorAt(const Position& position, bool invert) {
	const DoorSet* set = materials_.doorForItem(brush_id_);
	if (!set) {
		last_error_ = "Door brush needs a door item from materials.xml";
		return;
	}
	last_error_.clear();
	const WallSet* walls = materials_.walls().empty() ? nullptr : &materials_.walls().front();

	for (const Position& cell : BrushFootprint(position, brush_size_, map_.getWidth(), map_.getHeight())) {
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (stroke_seen_.count(key)) {
			continue;
		}
		stroke_seen_.insert(key);

		Tile after;
		if (const Tile* existing = map_.getTile(cell)) {
			after = existing->deepCopy();
		} else {
			after.setPosition(cell);
		}
		after.setPosition(cell);

		bool changed = false;
		if (invert) {
			changed = RemoveDoorFromTile(after, *set);
		} else {
			changed = ApplyDoorToTile(after, set->pieceFor(false), *set);
		}
		if (changed) {
			writeTileAfter(stroke_, cell, std::move(after));
		}
		if (invert) {
			continue;
		}
		const Tile* placed = map_.getTile(cell);
		if (!placed || !TileHasDoor(*placed, *set)) {
			continue;
		}
		Tile oriented = placed->deepCopy();
		if (ApplyDoorToTile(oriented, ResolveDoorPiece(map_, cell, *set, walls), *set)) {
			writeTileAfter(stroke_, cell, std::move(oriented));
		}
	}
}

void EditorSession::strokeTableAt(const Position& position, bool invert) {
	const TableSet* set = materials_.tableForItem(brush_id_);
	if (!set) {
		last_error_ = "Table brush needs a table item from materials.xml";
		return;
	}
	last_error_.clear();

	std::vector<Position> seeds;
	for (const Position& cell : BrushFootprint(position, brush_size_, map_.getWidth(), map_.getHeight())) {
		seeds.push_back(cell);
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (stroke_seen_.count(key)) {
			continue;
		}
		stroke_seen_.insert(key);

		Tile after;
		if (const Tile* existing = map_.getTile(cell)) {
			after = existing->deepCopy();
		} else {
			after.setPosition(cell);
		}
		after.setPosition(cell);

		bool changed = false;
		if (invert) {
			changed = RemoveTableFromTile(after, *set);
		} else if (!TileHasTable(after, *set)) {
			changed = ApplyTableToTile(after, set->pieceFor(0), *set);
		}
		if (changed) {
			writeTileAfter(stroke_, cell, std::move(after));
		}
	}

	auto restitchCell = [&](const Position& cell) {
		if (!map_.inBounds(cell)) {
			return;
		}
		const Tile* tile = map_.getTile(cell);
		if (!tile || !TileHasTable(*tile, *set)) {
			return;
		}
		Tile after = tile->deepCopy();
		if (ApplyTableToTile(after, ResolveTablePiece(map_, cell, *set), *set)) {
			writeTileAfter(stroke_, cell, std::move(after));
		}
	};

	constexpr int kDx[4] = {0, 1, 0, -1};
	constexpr int kDy[4] = {-1, 0, 1, 0};
	std::unordered_set<uint64_t> restitched;
	for (const Position& cell : seeds) {
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (restitched.insert(key).second) {
			restitchCell(cell);
		}
		for (int i = 0; i < 4; ++i) {
			const Position neighbor(cell.x + kDx[i], cell.y + kDy[i], cell.z);
			const uint64_t nkey = MakeTileKey(neighbor.x, neighbor.y, neighbor.z);
			if (restitched.insert(nkey).second) {
				restitchCell(neighbor);
			}
		}
	}
}

void EditorSession::strokeCarpetAt(const Position& position, bool invert) {
	const CarpetSet* set = materials_.carpetForItem(brush_id_);
	if (!set) {
		last_error_ = "Carpet brush needs a carpet item from materials.xml";
		return;
	}
	last_error_.clear();

	std::vector<Position> seeds;
	for (const Position& cell : BrushFootprint(position, brush_size_, map_.getWidth(), map_.getHeight())) {
		seeds.push_back(cell);
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (stroke_seen_.count(key)) {
			continue;
		}
		stroke_seen_.insert(key);

		Tile after;
		if (const Tile* existing = map_.getTile(cell)) {
			after = existing->deepCopy();
		} else {
			after.setPosition(cell);
		}
		after.setPosition(cell);

		bool changed = false;
		if (invert) {
			changed = RemoveCarpetFromTile(after, *set);
		} else if (!TileHasCarpet(after, *set)) {
			changed = ApplyCarpetToTile(after, set->inner_id != 0 ? set->inner_id : set->look_id, *set);
		}
		if (changed) {
			writeTileAfter(stroke_, cell, std::move(after));
		}
	}

	auto restitchCell = [&](const Position& cell) {
		if (!map_.inBounds(cell)) {
			return;
		}
		const Tile* tile = map_.getTile(cell);
		if (!tile || !TileHasCarpet(*tile, *set)) {
			return;
		}
		Tile after = tile->deepCopy();
		if (ApplyCarpetToTile(after, ResolveCarpetPiece(map_, cell, *set), *set)) {
			writeTileAfter(stroke_, cell, std::move(after));
		}
	};

	constexpr int kDx[4] = {0, 1, 0, -1};
	constexpr int kDy[4] = {-1, 0, 1, 0};
	std::unordered_set<uint64_t> restitched;
	for (const Position& cell : seeds) {
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (restitched.insert(key).second) {
			restitchCell(cell);
		}
		for (int i = 0; i < 4; ++i) {
			const Position neighbor(cell.x + kDx[i], cell.y + kDy[i], cell.z);
			const uint64_t nkey = MakeTileKey(neighbor.x, neighbor.y, neighbor.z);
			if (restitched.insert(nkey).second) {
				restitchCell(neighbor);
			}
		}
	}
}

void EditorSession::strokeAt(const Position& position, bool invert) {
	if (!stroking_) {
		beginStroke();
	}
	BrushKind kind = resolvedBrush();
	if (invert && kind != BrushKind::Flags && kind != BrushKind::House && kind != BrushKind::Wall
		&& kind != BrushKind::Border && kind != BrushKind::Doodad && kind != BrushKind::Door
		&& kind != BrushKind::Table && kind != BrushKind::Carpet) {
		kind = BrushKind::Eraser;
	}
	if (kind == BrushKind::Fill || kind == BrushKind::Select) {
		return;
	}
	if (kind == BrushKind::Wall) {
		strokeWallAt(position, invert);
		return;
	}
	if (kind == BrushKind::Border) {
		strokeBorderAt(position, invert);
		return;
	}
	if (kind == BrushKind::Doodad) {
		strokeDoodadAt(position, invert);
		return;
	}
	if (kind == BrushKind::Door) {
		strokeDoorAt(position, invert);
		return;
	}
	if (kind == BrushKind::Table) {
		strokeTableAt(position, invert);
		return;
	}
	if (kind == BrushKind::Carpet) {
		strokeCarpetAt(position, invert);
		return;
	}
	if (kind == BrushKind::House && house_id_ == 0) {
		house_id_ = addHouse("House", position);
	}
	for (const Position& cell : BrushFootprint(position, brush_size_, map_.getWidth(), map_.getHeight())) {
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (stroke_seen_.count(key)) {
			continue;
		}
		stroke_seen_.insert(key);
		applyLive(cell, kind, brush_id_, stroke_, invert && (kind == BrushKind::Flags || kind == BrushKind::House));
	}
}

void EditorSession::endStroke() {
	if (!stroking_) {
		return;
	}
	stroking_ = false;
	if (!stroke_.changes().empty()) {
		history_.record(std::move(stroke_));
	}
	stroke_ = Action(ActionIdentifier::BrushStroke);
	stroke_seen_.clear();
}

void EditorSession::fillAt(const Position& position) {
	endStroke();
	Action action(ActionIdentifier::Fill);
	uint16_t fill_id = brush_id_;
	const GroundBorderSet* borders = materials_.borderForItem(brush_id_);
	if (borders) {
		fill_id = borders->inner_id;
	}
	std::vector<Position> seeds;
	for (const Position& cell : FloodGround(map_, position)) {
		if (applyLive(cell, BrushKind::Ground, fill_id, action)) {
			seeds.push_back(cell);
		}
	}
	if (borders && !seeds.empty()) {
		restitchBorders(action, *borders, seeds);
	}
	if (action.changes().empty()) {
		last_error_ = "Fill did not change any tiles (need connected ground with a different brush id)";
		return;
	}
	last_error_.clear();
	history_.record(std::move(action));
}

bool EditorSession::pickAt(const Position& position) {
	setInspect(position);
	const Tile* tile = map_.getTile(position);
	if (!tile) {
		return false;
	}
	if (!tile->getItems().empty()) {
		brush_id_ = tile->getItems().back().getID();
		return true;
	}
	if (tile->hasGround()) {
		brush_id_ = tile->getGround()->getID();
		return true;
	}
	return false;
}

void EditorSession::deleteSelection() {
	endStroke();
	Action action(ActionIdentifier::Erase);
	for (const Position& cell : selection_.tiles()) {
		const Tile* existing = map_.getTile(cell);
		if (!existing || existing->empty()) {
			continue;
		}
		TileChange change;
		change.position = cell;
		change.before = existing->deepCopy();
		change.after.setPosition(cell);
		if (change.after.empty()) {
			map_.removeTile(cell);
		} else {
			map_.setTile(change.after.deepCopy());
		}
		action.addChange(std::move(change));
	}
	if (!action.changes().empty()) {
		markMinimapDirty();
		history_.record(std::move(action));
	}
}

void EditorSession::copySelection() {
	clipboard_.clear();
	const auto cells = selection_.tiles();
	if (cells.empty()) {
		return;
	}
	int min_x = cells.front().x;
	int min_y = cells.front().y;
	for (const Position& cell : cells) {
		min_x = std::min(min_x, cell.x);
		min_y = std::min(min_y, cell.y);
	}
	for (const Position& cell : cells) {
		const Tile* tile = map_.getTile(cell);
		if (!tile || tile->empty()) {
			continue;
		}
		ClipboardTile clip;
		clip.dx = cell.x - min_x;
		clip.dy = cell.y - min_y;
		clip.tile = tile->deepCopy();
		clipboard_.push_back(std::move(clip));
	}
}

void EditorSession::cutSelection() {
	copySelection();
	deleteSelection();
}

void EditorSession::pasteAt(const Position& position) {
	endStroke();
	if (clipboard_.empty()) {
		return;
	}
	Action action(ActionIdentifier::Paste);
	for (const ClipboardTile& clip : clipboard_) {
		const Position dest(position.x + clip.dx, position.y + clip.dy, position.z);
		if (!map_.inBounds(dest)) {
			continue;
		}
		TileChange change;
		change.position = dest;
		if (const Tile* existing = map_.getTile(dest)) {
			change.before = existing->deepCopy();
		} else {
			change.before.setPosition(dest);
		}
		change.after = clip.tile.deepCopy();
		change.after.setPosition(dest);
		if (change.after.empty()) {
			map_.removeTile(dest);
		} else {
			map_.setTile(change.after.deepCopy());
		}
		action.addChange(std::move(change));
	}
	if (!action.changes().empty()) {
		markMinimapDirty();
		history_.record(std::move(action));
	}
}

bool EditorSession::undo() {
	endStroke();
	const bool ok = history_.undo(map_);
	if (ok) {
		markMinimapDirty();
	}
	return ok;
}

bool EditorSession::redo() {
	endStroke();
	const bool ok = history_.redo(map_);
	if (ok) {
		markMinimapDirty();
	}
	return ok;
}

void EditorSession::setCamera(int x, int y) {
	camera_x_ = std::clamp(x, 0, std::max(0, map_.getWidth() - 1));
	camera_y_ = std::clamp(y, 0, std::max(0, map_.getHeight() - 1));
}

void EditorSession::panBy(int dx, int dy) {
	setCamera(camera_x_ + dx, camera_y_ + dy);
}

void EditorSession::goTo(int x, int y, int z) {
	setFloor(z);
	setCamera(x, y);
	setInspect(Position(camera_x_, camera_y_, floor_));
}

void EditorSession::setFloor(int floor) {
	const int next = std::clamp(floor, rme::MapMinLayer, rme::MapMaxLayer);
	if (next != floor_) {
		floor_ = next;
		markMinimapDirty();
	}
}

void EditorSession::centerOnOccupied() {
	if (map_.tileCount() == 0) {
		setCamera(map_.getWidth() / 2, map_.getHeight() / 2);
		return;
	}

	int floor_counts[rme::MapLayers] = {};
	for (const auto& [_, tile] : map_.tiles()) {
		const int z = tile.getPosition().z;
		if (z >= rme::MapMinLayer && z <= rme::MapMaxLayer) {
			++floor_counts[z];
		}
	}
	int preferred_floor = rme::MapGroundLayer;
	if (floor_counts[rme::MapGroundLayer] == 0) {
		int best = 0;
		for (int z = rme::MapMinLayer; z <= rme::MapMaxLayer; ++z) {
			if (floor_counts[z] > best) {
				best = floor_counts[z];
				preferred_floor = z;
			}
		}
	}

	long long sx = 0;
	long long sy = 0;
	int count = 0;
	for (const auto& [_, tile] : map_.tiles()) {
		if (tile.getPosition().z != preferred_floor) {
			continue;
		}
		sx += tile.getPosition().x;
		sy += tile.getPosition().y;
		++count;
	}
	if (count > 0) {
		setCamera(static_cast<int>(sx / count), static_cast<int>(sy / count));
		setFloor(preferred_floor);
		setInspect(Position(camera_x_, camera_y_, floor_));
	}
}

uint16_t EditorSession::spriteIdForItem(uint16_t item_id) const {
	if (const ItemType* type = items_.get(item_id)) {
		return type->sprite_id;
	}
	return 0;
}

const Minimap& EditorSession::minimap() {
	if (minimap_dirty_ || minimap_.width() != map_.getWidth() || minimap_.height() != map_.getHeight()
		|| minimap_.floor() != floor_) {
		minimap_.rebuild(map_, items_, floor_);
		minimap_dirty_ = false;
	}
	return minimap_;
}

uint32_t EditorSession::addTown(std::string name, const Position& temple) {
	uint32_t id = 1;
	for (const Town& town : map_.towns()) {
		id = std::max(id, town.id + 1);
	}
	Town town;
	town.id = id;
	town.name = name.empty() ? ("Town " + std::to_string(id)) : std::move(name);
	town.temple = temple;
	map_.towns().push_back(std::move(town));
	map_.markChanged();
	return id;
}

bool EditorSession::removeTown(uint32_t id) {
	auto& towns = map_.towns();
	const auto it = std::remove_if(towns.begin(), towns.end(), [id](const Town& town) {
		return town.id == id;
	});
	if (it == towns.end()) {
		return false;
	}
	towns.erase(it, towns.end());
	map_.markChanged();
	return true;
}

bool EditorSession::renameTown(uint32_t id, std::string name) {
	for (Town& town : map_.towns()) {
		if (town.id == id) {
			town.name = std::move(name);
			map_.markChanged();
			return true;
		}
	}
	return false;
}

bool EditorSession::goToTown(uint32_t id) {
	for (const Town& town : map_.towns()) {
		if (town.id == id) {
			setCamera(town.temple.x, town.temple.y);
			setFloor(town.temple.z);
			return true;
		}
	}
	return false;
}

void EditorSession::addWaypoint(std::string name, const Position& position) {
	Waypoint waypoint;
	waypoint.name = name.empty() ? ("wp" + std::to_string(map_.waypoints().size() + 1)) : std::move(name);
	waypoint.position = position;
	map_.waypoints().push_back(std::move(waypoint));
	map_.markChanged();
}

bool EditorSession::removeWaypoint(std::size_t index) {
	if (index >= map_.waypoints().size()) {
		return false;
	}
	map_.waypoints().erase(map_.waypoints().begin() + static_cast<std::ptrdiff_t>(index));
	map_.markChanged();
	return true;
}

bool EditorSession::goToWaypoint(std::size_t index) {
	if (index >= map_.waypoints().size()) {
		return false;
	}
	const Waypoint& waypoint = map_.waypoints()[index];
	setCamera(waypoint.position.x, waypoint.position.y);
	setFloor(waypoint.position.z);
	return true;
}

uint32_t EditorSession::addHouse(std::string name, const Position& entry) {
	uint32_t id = 1;
	for (const House& house : map_.houses()) {
		id = std::max(id, house.id + 1);
	}
	House house;
	house.id = id;
	house.name = name.empty() ? ("House " + std::to_string(id)) : std::move(name);
	house.entry = entry;
	if (!map_.towns().empty()) {
		house.town_id = map_.towns().front().id;
	}
	map_.houses().push_back(std::move(house));
	house_id_ = id;
	map_.markChanged();
	return id;
}

bool EditorSession::removeHouse(uint32_t id) {
	auto& houses = map_.houses();
	const auto it = std::remove_if(houses.begin(), houses.end(), [id](const House& house) {
		return house.id == id;
	});
	if (it == houses.end()) {
		return false;
	}
	houses.erase(it, houses.end());
	if (house_id_ == id) {
		house_id_ = houses.empty() ? 0 : houses.front().id;
	}
	map_.markChanged();
	return true;
}

bool EditorSession::renameHouse(uint32_t id, std::string name) {
	for (House& house : map_.houses()) {
		if (house.id == id) {
			house.name = std::move(name);
			map_.markChanged();
			return true;
		}
	}
	return false;
}

bool EditorSession::goToHouse(uint32_t id) {
	for (const House& house : map_.houses()) {
		if (house.id == id) {
			goTo(house.entry.x, house.entry.y, house.entry.z);
			return true;
		}
	}
	return false;
}

std::size_t EditorSession::addSpawn(const Position& center, int radius) {
	Spawn spawn;
	spawn.center = center;
	spawn.radius = std::clamp(radius, 1, 16);
	SpawnCreature rat;
	rat.name = "Rat";
	spawn.monsters.push_back(std::move(rat));
	map_.spawns().push_back(std::move(spawn));
	map_.markChanged();
	return map_.spawns().size() - 1;
}

bool EditorSession::removeSpawn(std::size_t index) {
	if (index >= map_.spawns().size()) {
		return false;
	}
	map_.spawns().erase(map_.spawns().begin() + static_cast<std::ptrdiff_t>(index));
	map_.markChanged();
	return true;
}

bool EditorSession::goToSpawn(std::size_t index) {
	if (index >= map_.spawns().size()) {
		return false;
	}
	const Spawn& spawn = map_.spawns()[index];
	goTo(spawn.center.x, spawn.center.y, spawn.center.z);
	return true;
}

bool EditorSession::addSpawnMonster(std::size_t spawn_index, std::string name, int dx, int dy, uint32_t spawntime) {
	if (spawn_index >= map_.spawns().size()) {
		return false;
	}
	SpawnCreature creature;
	creature.name = name.empty() ? "Monster" : std::move(name);
	creature.dx = dx;
	creature.dy = dy;
	creature.spawntime = std::max(1u, spawntime);
	map_.spawns()[spawn_index].monsters.push_back(std::move(creature));
	map_.markChanged();
	return true;
}

void EditorSession::setInspect(const Position& position) {
	inspect_ = position;
	inspect_index_ = -1;
}

void EditorSession::clampInspectIndex() {
	const Tile* tile = map_.getTile(inspect_);
	if (!tile || tile->stackCount() == 0) {
		inspect_index_ = 0;
		return;
	}
	if (inspect_index_ < 0 || inspect_index_ >= tile->stackCount()) {
		inspect_index_ = tile->stackCount() - 1;
	}
}

int EditorSession::inspectIndex() const {
	const Tile* tile = map_.getTile(inspect_);
	if (!tile || tile->stackCount() == 0) {
		return 0;
	}
	if (inspect_index_ < 0 || inspect_index_ >= tile->stackCount()) {
		return tile->stackCount() - 1;
	}
	return inspect_index_;
}

void EditorSession::setInspectIndex(int index) {
	inspect_index_ = index;
	clampInspectIndex();
}

const Item* EditorSession::inspectItem() const {
	const Tile* tile = map_.getTile(inspect_);
	return tile ? tile->stackItem(inspectIndex()) : nullptr;
}

Item* EditorSession::inspectItem() {
	Tile* tile = map_.getTile(inspect_);
	return tile ? tile->stackItem(inspectIndex()) : nullptr;
}

std::vector<StackEntry> EditorSession::browseInspect() const {
	std::vector<StackEntry> entries;
	const Tile* tile = map_.getTile(inspect_);
	if (!tile) {
		return entries;
	}
	for (int i = 0; i < tile->stackCount(); ++i) {
		const Item* item = tile->stackItem(i);
		if (!item) {
			continue;
		}
		StackEntry entry;
		entry.index = i;
		entry.item_id = item->getID();
		entry.is_ground = tile->isGroundIndex(i);
		entry.content_count = item->getContents().size();
		entry.action_id = item->getActionID();
		entry.unique_id = item->getUniqueID();
		entries.push_back(entry);
	}
	return entries;
}

bool EditorSession::recordInspectTile(Tile after) {
	Tile* tile = map_.getTile(inspect_);
	if (!tile) {
		last_error_ = "No inspect tile";
		return false;
	}
	Action action(ActionIdentifier::Replace);
	TileChange change;
	change.position = inspect_;
	change.before = tile->deepCopy();
	change.after = std::move(after);
	if (change.after.empty()) {
		map_.removeTile(inspect_);
	} else {
		map_.setTile(change.after.deepCopy());
	}
	action.addChange(std::move(change));
	history_.record(std::move(action));
	clampInspectIndex();
	last_error_.clear();
	return true;
}

bool EditorSession::editTopItem(const ItemProps& props) {
	endStroke();
	Tile* tile = map_.getTile(inspect_);
	const int index = inspectIndex();
	if (!tile || !tile->stackItem(index)) {
		last_error_ = "No item at inspect tile";
		return false;
	}
	ItemProps normalized = props;
	if (normalized.count == 0) {
		normalized.count = 1;
	}
	if (PropsEqual(PropsFromItem(*tile->stackItem(index)), normalized)) {
		return false;
	}

	Tile after = tile->deepCopy();
	ApplyPropsToItem(*after.stackItem(index), normalized);
	return recordInspectTile(std::move(after));
}

bool EditorSession::removeInspectItem() {
	endStroke();
	Tile* tile = map_.getTile(inspect_);
	const int index = inspectIndex();
	if (!tile || !tile->stackItem(index)) {
		last_error_ = "No item to remove";
		return false;
	}
	Tile after = tile->deepCopy();
	after.removeStackIndex(index);
	return recordInspectTile(std::move(after));
}

bool EditorSession::moveInspectItem(int delta) {
	endStroke();
	Tile* tile = map_.getTile(inspect_);
	const int index = inspectIndex();
	if (!tile || tile->isGroundIndex(index)) {
		last_error_ = "Ground cannot move in the overlay stack";
		return false;
	}
	int overlay = index;
	if (tile->hasGround()) {
		--overlay;
	}
	Tile after = tile->deepCopy();
	if (!after.moveOverlay(overlay, delta)) {
		last_error_ = "Cannot move item further";
		return false;
	}
	if (!recordInspectTile(std::move(after))) {
		return false;
	}
	inspect_index_ = index + delta;
	clampInspectIndex();
	return true;
}

bool EditorSession::addContainerItem(uint16_t item_id, uint8_t count) {
	endStroke();
	Tile* tile = map_.getTile(inspect_);
	const int index = inspectIndex();
	if (!tile || !tile->stackItem(index) || item_id == 0) {
		last_error_ = "Select a container item first";
		return false;
	}
	Tile after = tile->deepCopy();
	Item nested(item_id);
	nested.setCount(count == 0 ? 1 : count);
	after.stackItem(index)->getContents().push_back(std::move(nested));
	return recordInspectTile(std::move(after));
}

bool EditorSession::removeContainerItem(std::size_t nested_index) {
	endStroke();
	Tile* tile = map_.getTile(inspect_);
	const int index = inspectIndex();
	if (!tile || !tile->stackItem(index)) {
		last_error_ = "No container item selected";
		return false;
	}
	Tile after = tile->deepCopy();
	auto& contents = after.stackItem(index)->getContents();
	if (nested_index >= contents.size()) {
		last_error_ = "Container slot out of range";
		return false;
	}
	contents.erase(contents.begin() + static_cast<std::ptrdiff_t>(nested_index));
	return recordInspectTile(std::move(after));
}

namespace {

bool TileMatchesQuery(const Tile& tile, const FindQuery& query) {
	if (tile.getGround() && ItemMatchesQueryDeep(*tile.getGround(), query)) {
		return true;
	}
	for (const Item& item : tile.getItems()) {
		if (ItemMatchesQueryDeep(item, query)) {
			return true;
		}
	}
	return false;
}

bool PositionLess(const Position& a, const Position& b) {
	if (a.z != b.z) {
		return a.z < b.z;
	}
	if (a.y != b.y) {
		return a.y < b.y;
	}
	return a.x < b.x;
}

} // namespace

std::vector<Position> EditorSession::findTiles(const FindQuery& query) const {
	std::vector<Position> hits;
	if (query.empty()) {
		return hits;
	}
	for (const auto& [_, tile] : map_.tiles()) {
		if (TileMatchesQuery(tile, query)) {
			hits.push_back(tile.getPosition());
		}
	}
	std::sort(hits.begin(), hits.end(), PositionLess);
	return hits;
}

bool EditorSession::findNext(const FindQuery& query) {
	const auto hits = findTiles(query);
	if (hits.empty()) {
		last_error_ = "No matching items";
		return false;
	}
	auto it = std::upper_bound(hits.begin(), hits.end(), inspect_, PositionLess);
	if (it == hits.end()) {
		it = hits.begin();
	}
	goTo(it->x, it->y, it->z);
	last_error_.clear();
	return true;
}

std::vector<MapIssue> EditorSession::mapIssues() const {
	std::vector<MapIssue> issues;
	std::unordered_map<uint16_t, Position> uids;

	auto consider = [&](const Position& pos, const Item& item) {
		if (item.getID() != 0 && items_.get(item.getID()) == nullptr) {
			MapIssue issue;
			issue.kind = MapIssueKind::UnknownItem;
			issue.position = pos;
			issue.item_id = item.getID();
			issue.message = "Unknown item id " + std::to_string(item.getID());
			issues.push_back(std::move(issue));
		}
		if (item.getUniqueID() != 0) {
			const auto it = uids.find(item.getUniqueID());
			if (it != uids.end()) {
				MapIssue issue;
				issue.kind = MapIssueKind::DuplicateUniqueId;
				issue.position = pos;
				issue.item_id = item.getID();
				issue.unique_id = item.getUniqueID();
				std::ostringstream text;
				text << "UID " << item.getUniqueID() << " also at " << it->second.x << "," << it->second.y << ","
					 << it->second.z;
				issue.message = text.str();
				issues.push_back(std::move(issue));
			} else {
				uids.emplace(item.getUniqueID(), pos);
			}
		}
		if (item.hasDestination()) {
			const Position dest = item.getDestination();
			if (!map_.inBounds(dest) || map_.getTile(dest) == nullptr) {
				MapIssue issue;
				issue.kind = MapIssueKind::InvalidTeleport;
				issue.position = pos;
				issue.item_id = item.getID();
				std::ostringstream text;
				text << "Teleport dest " << dest.x << "," << dest.y << "," << dest.z << " is empty";
				issue.message = text.str();
				issues.push_back(std::move(issue));
			}
		}
	};

	std::function<void(const Position&, const Item&)> walk;
	walk = [&](const Position& pos, const Item& item) {
		consider(pos, item);
		for (const Item& nested : item.getContents()) {
			walk(pos, nested);
		}
	};

	for (const auto& [_, tile] : map_.tiles()) {
		if (tile.getGround()) {
			walk(tile.getPosition(), *tile.getGround());
		}
		for (const Item& item : tile.getItems()) {
			walk(tile.getPosition(), item);
		}
	}

	std::sort(issues.begin(), issues.end(), [](const MapIssue& a, const MapIssue& b) {
		if (a.position.z != b.position.z) {
			return a.position.z < b.position.z;
		}
		if (a.position.y != b.position.y) {
			return a.position.y < b.position.y;
		}
		if (a.position.x != b.position.x) {
			return a.position.x < b.position.x;
		}
		return static_cast<int>(a.kind) < static_cast<int>(b.kind);
	});
	return issues;
}

bool EditorSession::goToIssue(std::size_t index) {
	const auto issues = mapIssues();
	if (index >= issues.size()) {
		last_error_ = "No such map issue";
		return false;
	}
	goTo(issues[index].position.x, issues[index].position.y, issues[index].position.z);
	last_error_.clear();
	return true;
}

} // namespace core
} // namespace rme
