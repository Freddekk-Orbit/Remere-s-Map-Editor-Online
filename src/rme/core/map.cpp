#include "map.h"

#include <algorithm>

namespace rme {
namespace core {

Map::Map() {
	createEmpty(256, 256);
}

void Map::clear() {
	tiles_.clear();
	towns_.clear();
	waypoints_.clear();
	clearMessages();
	changed_ = false;
}

void Map::createEmpty(int width, int height, const std::string& name) {
	clear();
	width_ = std::max(rme::MapMinWidth, width);
	height_ = std::max(rme::MapMinHeight, height);
	name_ = name;
	description_ = "No map description available.";
	house_file_.clear();
	spawn_file_.clear();
	spawn_npc_file_.clear();
	zone_file_.clear();
	version_ = MapVersion{};
}

void Map::setSize(int width, int height) {
	width_ = width;
	height_ = height;
	markChanged();
}

bool Map::inBounds(const Position& position) const {
	return position.x >= 0 && position.y >= 0 && position.x < width_ && position.y < height_
		&& position.z >= rme::MapMinLayer && position.z <= rme::MapMaxLayer;
}

Tile* Map::getTile(const Position& position) {
	const auto it = tiles_.find(MakeTileKey(position.x, position.y, position.z));
	return it == tiles_.end() ? nullptr : &it->second;
}

const Tile* Map::getTile(const Position& position) const {
	const auto it = tiles_.find(MakeTileKey(position.x, position.y, position.z));
	return it == tiles_.end() ? nullptr : &it->second;
}

Tile& Map::ensureTile(const Position& position) {
	const auto key = MakeTileKey(position.x, position.y, position.z);
	auto [it, inserted] = tiles_.try_emplace(key, position);
	if (inserted) {
		it->second.setPosition(position);
	}
	return it->second;
}

void Map::setTile(Tile tile) {
	const Position pos = tile.getPosition();
	if (tile.empty()) {
		removeTile(pos);
		return;
	}
	tiles_[MakeTileKey(pos.x, pos.y, pos.z)] = std::move(tile);
	markChanged();
}

void Map::removeTile(const Position& position) {
	tiles_.erase(MakeTileKey(position.x, position.y, position.z));
	markChanged();
}

Tile Map::swapTile(const Position& position, Tile replacement) {
	Tile previous;
	if (const Tile* existing = getTile(position)) {
		previous = existing->deepCopy();
	} else {
		previous.setPosition(position);
	}
	setTile(std::move(replacement));
	return previous;
}

std::size_t Map::itemCount() const {
	std::size_t count = 0;
	for (const auto& [_, tile] : tiles_) {
		count += tile.itemCount();
	}
	return count;
}

std::size_t Map::countTilesOnFloor(int z) const {
	std::size_t count = 0;
	for (const auto& [_, tile] : tiles_) {
		if (tile.getPosition().z == z) {
			++count;
		}
	}
	return count;
}

std::vector<Position> Map::occupiedPositions() const {
	std::vector<Position> positions;
	positions.reserve(tiles_.size());
	for (const auto& [_, tile] : tiles_) {
		positions.push_back(tile.getPosition());
	}
	return positions;
}

} // namespace core
} // namespace rme
