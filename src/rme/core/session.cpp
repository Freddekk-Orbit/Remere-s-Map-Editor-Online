#include "session.h"

#include <algorithm>
#include <filesystem>

namespace rme {
namespace core {

EditorSession::EditorSession() {
	newMap();
}

void EditorSession::newMap(int width, int height) {
	map_.createEmpty(width, height);
	history_.clear();
	last_error_.clear();
	camera_x_ = width / 2;
	camera_y_ = height / 2;
	floor_ = rme::MapGroundLayer;
}

bool EditorSession::loadOtbm(const std::string& path) {
	Map loaded;
	if (!LoadOTBM(loaded, path)) {
		last_error_ = loaded.getError().empty() ? "Failed to load OTBM" : loaded.getError();
		return false;
	}
	map_ = std::move(loaded);
	history_.clear();
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
	if (!LoadDatHeader(path, assets_)) {
		last_error_ = assets_.error.empty() ? "Failed to read .dat" : assets_.error;
		return false;
	}
	last_error_.clear();
	return true;
}

bool EditorSession::loadSpr(const std::string& path) {
	if (!LoadSprHeader(path, assets_)) {
		last_error_ = assets_.error.empty() ? "Failed to read .spr" : assets_.error;
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

void EditorSession::paintGround(const Position& position, uint16_t item_id) {
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

bool EditorSession::undo() {
	return history_.undo(map_);
}

bool EditorSession::redo() {
	return history_.redo(map_);
}

void EditorSession::setCamera(int x, int y) {
	camera_x_ = std::clamp(x, 0, std::max(0, map_.getWidth() - 1));
	camera_y_ = std::clamp(y, 0, std::max(0, map_.getHeight() - 1));
}

void EditorSession::setFloor(int floor) {
	floor_ = std::clamp(floor, rme::MapMinLayer, rme::MapMaxLayer);
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

} // namespace core
} // namespace rme
