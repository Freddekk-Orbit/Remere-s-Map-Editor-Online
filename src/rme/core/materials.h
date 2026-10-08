#pragma once

#include "map.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rme {
namespace core {

struct WallSet {
	std::string name = "Wall";
	uint16_t look_id = 0;
	std::array<uint16_t, 16> piece{};

	bool contains(uint16_t item_id) const;
	uint16_t pieceFor(uint8_t mask) const;
	void fillFromParts(uint16_t pole, uint16_t horizontal, uint16_t vertical, uint16_t junction);
};

struct Tileset {
	std::string name;
	std::vector<uint16_t> items;
};

class Materials {
public:
	void clear();
	bool load(const std::string& path);
	bool writeSample(const std::string& path);
	void ensureDefaults();

	const std::vector<Tileset>& tilesets() const { return tilesets_; }
	const std::vector<WallSet>& walls() const { return walls_; }
	const WallSet* wallForItem(uint16_t item_id) const;
	const std::string& lastError() const { return error_; }

private:
	std::vector<Tileset> tilesets_;
	std::vector<WallSet> walls_;
	std::string error_;
};

enum {
	kWallNorth = 1,
	kWallEast = 2,
	kWallSouth = 4,
	kWallWest = 8,
};

bool TileHasWall(const Tile& tile, const WallSet& set);
uint8_t WallNeighborMask(const Map& map, const Position& position, const WallSet& set);
uint16_t ResolveWallPiece(const Map& map, const Position& position, const WallSet& set);
bool ApplyWallToTile(Tile& tile, uint16_t piece_id, const WallSet& set);
bool RemoveWallFromTile(Tile& tile, const WallSet& set);

} // namespace core
} // namespace rme
