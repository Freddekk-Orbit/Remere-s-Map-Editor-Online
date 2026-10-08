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

struct GroundBorderSet {
	std::string name = "Water";
	uint16_t inner_id = 0;
	uint16_t edge_n = 0;
	uint16_t edge_e = 0;
	uint16_t edge_s = 0;
	uint16_t edge_w = 0;
	uint16_t corner_ne = 0;
	uint16_t corner_se = 0;
	uint16_t corner_sw = 0;
	uint16_t corner_nw = 0;

	bool containsInner(uint16_t item_id) const { return inner_id != 0 && item_id == inner_id; }
	bool containsBorder(uint16_t item_id) const;
	bool contains(uint16_t item_id) const { return containsInner(item_id) || containsBorder(item_id); }
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
	const std::vector<GroundBorderSet>& borders() const { return borders_; }
	const WallSet* wallForItem(uint16_t item_id) const;
	const GroundBorderSet* borderForItem(uint16_t item_id) const;
	const std::string& lastError() const { return error_; }

private:
	std::vector<Tileset> tilesets_;
	std::vector<WallSet> walls_;
	std::vector<GroundBorderSet> borders_;
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

bool TileHasInnerGround(const Tile& tile, const GroundBorderSet& set);
std::vector<uint16_t> ResolveBorderPieces(const Map& map, const Position& position, const GroundBorderSet& set);
bool ApplyBordersToTile(Tile& tile, const Map& map, const GroundBorderSet& set);
bool RemoveBordersFromTile(Tile& tile, const GroundBorderSet& set);

} // namespace core
} // namespace rme
