#include "session.h"

#include <algorithm>
#include <filesystem>

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
	last_error_.clear();
	return true;
}

bool EditorSession::saveOtbm(const std::string& path) {
	if (!SaveOTBM(map_, path)) {
		last_error_ = "Failed to write " + path;
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
	return ResolveBrush(brush_kind_, items_.get(brush_id_));
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
	if (!ApplyBrushToTile(change.after, kind, item_id, flag_mask_, invert && kind == BrushKind::Flags)) {
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

void EditorSession::strokeAt(const Position& position, bool invert) {
	if (!stroking_) {
		beginStroke();
	}
	const BrushKind kind = invert && brush_kind_ != BrushKind::Flags ? BrushKind::Eraser : resolvedBrush();
	if (kind == BrushKind::Fill || kind == BrushKind::Select) {
		return;
	}
	for (const Position& cell : BrushFootprint(position, brush_size_, map_.getWidth(), map_.getHeight())) {
		const uint64_t key = MakeTileKey(cell.x, cell.y, cell.z);
		if (stroke_seen_.count(key)) {
			continue;
		}
		stroke_seen_.insert(key);
		applyLive(cell, kind, brush_id_, stroke_, invert && kind == BrushKind::Flags);
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
	for (const Position& cell : FloodGround(map_, position)) {
		applyLive(cell, BrushKind::Ground, brush_id_, action);
	}
	if (action.changes().empty()) {
		last_error_ = "Fill did not change any tiles (need connected ground with a different brush id)";
		return;
	}
	last_error_.clear();
	history_.record(std::move(action));
}

bool EditorSession::pickAt(const Position& position) {
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
	long long sx = 0;
	long long sy = 0;
	int count = 0;
	int preferred_floor = floor_;
	for (const auto& [_, tile] : map_.tiles()) {
		sx += tile.getPosition().x;
		sy += tile.getPosition().y;
		preferred_floor = tile.getPosition().z;
		++count;
	}
	if (count > 0) {
		setCamera(static_cast<int>(sx / count), static_cast<int>(sy / count));
		setFloor(preferred_floor);
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

} // namespace core
} // namespace rme
