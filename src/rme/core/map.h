#pragma once

#include "tile.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rme {
namespace core {

struct MapVersion {
	uint32_t otbm = 2;
	uint32_t items_major = 3;
	uint32_t items_minor = 57;
};

struct Town {
	uint32_t id = 0;
	std::string name;
	Position temple;
};

struct Waypoint {
	std::string name;
	Position position;
};

inline uint64_t MakeTileKey(int x, int y, int z) {
	return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32)
		| (static_cast<uint64_t>(static_cast<uint16_t>(y)) << 16)
		| static_cast<uint16_t>(static_cast<uint8_t>(z));
}

class Map {
public:
	Map();

	void clear();
	void createEmpty(int width, int height, const std::string& name = "Untitled.otbm");

	int getWidth() const { return width_; }
	int getHeight() const { return height_; }
	void setSize(int width, int height);

	const MapVersion& getVersion() const { return version_; }
	void setVersion(const MapVersion& version) { version_ = version; }

	const std::string& getName() const { return name_; }
	void setName(std::string name) { name_ = std::move(name); }
	const std::string& getDescription() const { return description_; }
	void setDescription(std::string description) { description_ = std::move(description); }
	const std::string& getHouseFilename() const { return house_file_; }
	void setHouseFilename(std::string name) { house_file_ = std::move(name); }
	const std::string& getSpawnFilename() const { return spawn_file_; }
	void setSpawnFilename(std::string name) { spawn_file_ = std::move(name); }
	const std::string& getSpawnNpcFilename() const { return spawn_npc_file_; }
	void setSpawnNpcFilename(std::string name) { spawn_npc_file_ = std::move(name); }
	const std::string& getZoneFilename() const { return zone_file_; }
	void setZoneFilename(std::string name) { zone_file_ = std::move(name); }

	bool hasChanged() const { return changed_; }
	void markChanged() { changed_ = true; }
	void clearChanges() { changed_ = false; }

	bool inBounds(const Position& position) const;

	Tile* getTile(const Position& position);
	const Tile* getTile(const Position& position) const;
	Tile& ensureTile(const Position& position);
	void setTile(Tile tile);
	void removeTile(const Position& position);
	Tile swapTile(const Position& position, Tile replacement);

	std::size_t tileCount() const { return tiles_.size(); }
	std::size_t itemCount() const;
	std::size_t countTilesOnFloor(int z) const;

	const std::unordered_map<uint64_t, Tile>& tiles() const { return tiles_; }
	std::vector<Position> occupiedPositions() const;

	std::vector<Town>& towns() { return towns_; }
	const std::vector<Town>& towns() const { return towns_; }
	std::vector<Waypoint>& waypoints() { return waypoints_; }
	const std::vector<Waypoint>& waypoints() const { return waypoints_; }

	const std::string& getError() const { return error_; }
	void setError(std::string error) { error_ = std::move(error); }
	const std::vector<std::string>& getWarnings() const { return warnings_; }
	void addWarning(std::string warning) { warnings_.push_back(std::move(warning)); }
	void clearMessages() {
		error_.clear();
		warnings_.clear();
	}

	int width_ = 256;
	int height_ = 256;

private:
	MapVersion version_;
	std::string name_ = "Untitled.otbm";
	std::string description_ = "No map description available.";
	std::string house_file_;
	std::string spawn_file_;
	std::string spawn_npc_file_;
	std::string zone_file_;
	std::unordered_map<uint64_t, Tile> tiles_;
	std::vector<Town> towns_;
	std::vector<Waypoint> waypoints_;
	bool changed_ = false;
	std::string error_;
	std::vector<std::string> warnings_;
};

} // namespace core
} // namespace rme
