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

struct DoodadSet {
	std::string name = "Doodad";
	uint16_t look_id = 0;
	int chance = 60;
	std::vector<uint16_t> items;

	bool contains(uint16_t item_id) const;
	uint16_t pick(const Position& position) const;
};

struct DoorSet {
	std::string name = "Door";
	uint16_t look_id = 0;
	uint16_t horizontal = 0;
	uint16_t vertical = 0;

	bool contains(uint16_t item_id) const;
	uint16_t pieceFor(bool horizontal_door) const;
};

struct TableSet {
	std::string name = "Table";
	uint16_t look_id = 0;
	std::array<uint16_t, 16> piece{};

	bool contains(uint16_t item_id) const;
	uint16_t pieceFor(uint8_t mask) const;
	void fillFromParts(uint16_t pole, uint16_t horizontal, uint16_t vertical, uint16_t junction);
};

struct CarpetSet {
	std::string name = "Carpet";
	uint16_t look_id = 0;
	uint16_t inner_id = 0;
	uint16_t edge_n = 0;
	uint16_t edge_e = 0;
	uint16_t edge_s = 0;
	uint16_t edge_w = 0;
	uint16_t corner_ne = 0;
	uint16_t corner_se = 0;
	uint16_t corner_sw = 0;
	uint16_t corner_nw = 0;

	bool contains(uint16_t item_id) const;
	uint16_t pieceFor(bool n, bool e, bool s, bool w) const;
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
	const std::vector<DoodadSet>& doodads() const { return doodads_; }
	const std::vector<DoorSet>& doors() const { return doors_; }
	const std::vector<TableSet>& tables() const { return tables_; }
	const std::vector<CarpetSet>& carpets() const { return carpets_; }
	const WallSet* wallForItem(uint16_t item_id) const;
	const GroundBorderSet* borderForItem(uint16_t item_id) const;
	const DoodadSet* doodadForItem(uint16_t item_id) const;
	const DoorSet* doorForItem(uint16_t item_id) const;
	const TableSet* tableForItem(uint16_t item_id) const;
	const CarpetSet* carpetForItem(uint16_t item_id) const;
	const std::string& lastError() const { return error_; }

private:
	std::vector<Tileset> tilesets_;
	std::vector<WallSet> walls_;
	std::vector<GroundBorderSet> borders_;
	std::vector<DoodadSet> doodads_;
	std::vector<DoorSet> doors_;
	std::vector<TableSet> tables_;
	std::vector<CarpetSet> carpets_;
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

bool DoodadHits(const Position& position, int chance);
bool ApplyDoodadToTile(Tile& tile, uint16_t item_id);
bool RemoveDoodadFromTile(Tile& tile, const DoodadSet& set);

bool TileHasDoor(const Tile& tile, const DoorSet& set);
uint16_t ResolveDoorPiece(const Map& map, const Position& position, const DoorSet& set, const WallSet* walls);
bool ApplyDoorToTile(Tile& tile, uint16_t piece_id, const DoorSet& set);
bool RemoveDoorFromTile(Tile& tile, const DoorSet& set);

bool TileHasTable(const Tile& tile, const TableSet& set);
uint8_t TableNeighborMask(const Map& map, const Position& position, const TableSet& set);
uint16_t ResolveTablePiece(const Map& map, const Position& position, const TableSet& set);
bool ApplyTableToTile(Tile& tile, uint16_t piece_id, const TableSet& set);
bool RemoveTableFromTile(Tile& tile, const TableSet& set);

bool TileHasCarpet(const Tile& tile, const CarpetSet& set);
uint16_t ResolveCarpetPiece(const Map& map, const Position& position, const CarpetSet& set);
bool ApplyCarpetToTile(Tile& tile, uint16_t piece_id, const CarpetSet& set);
bool RemoveCarpetFromTile(Tile& tile, const CarpetSet& set);

} // namespace core
} // namespace rme
