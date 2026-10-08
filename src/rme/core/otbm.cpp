#include "otbm.h"
#include "map_xml.h"

#include <cstring>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace rme {
namespace core {
namespace {

bool LoadItem(BinaryNode* node, Item& item) {
	uint16_t id = 0;
	if (!node->getU16(id)) {
		return false;
	}
	item.setID(id);

	uint8_t attribute = 0;
	while (node->getU8(attribute)) {
		switch (attribute) {
			case OTBM_ATTR_COUNT: {
				uint8_t count = 1;
				if (!node->getU8(count)) {
					return false;
				}
				item.setCount(count);
				break;
			}
			case OTBM_ATTR_ACTION_ID: {
				uint16_t value = 0;
				if (!node->getU16(value)) {
					return false;
				}
				item.setActionID(value);
				break;
			}
			case OTBM_ATTR_UNIQUE_ID: {
				uint16_t value = 0;
				if (!node->getU16(value)) {
					return false;
				}
				item.setUniqueID(value);
				break;
			}
			case OTBM_ATTR_CHARGES:
			case OTBM_ATTR_RUNE_CHARGES: {
				uint16_t value = 0;
				if (!node->getU16(value)) {
					return false;
				}
				item.setCharges(value);
				break;
			}
			case OTBM_ATTR_TEXT:
			case OTBM_ATTR_WRITTENBY: {
				std::string text;
				if (!node->getString(text)) {
					return false;
				}
				item.setText(std::move(text));
				break;
			}
			case OTBM_ATTR_DESC: {
				std::string text;
				if (!node->getString(text)) {
					return false;
				}
				item.setDescription(std::move(text));
				break;
			}
			case OTBM_ATTR_TELE_DEST: {
				uint16_t x = 0, y = 0;
				uint8_t z = 0;
				if (!node->getU16(x) || !node->getU16(y) || !node->getU8(z)) {
					return false;
				}
				item.setDestination(Position(x, y, z));
				break;
			}
			case OTBM_ATTR_DEPOT_ID: {
				uint16_t value = 0;
				if (!node->getU16(value)) {
					return false;
				}
				item.setDepotID(value);
				break;
			}
			case OTBM_ATTR_HOUSEDOORID: {
				uint8_t value = 0;
				if (!node->getU8(value)) {
					return false;
				}
				item.setDoorID(value);
				break;
			}
			case OTBM_ATTR_DURATION:
			case OTBM_ATTR_DECAYING_STATE:
			case OTBM_ATTR_WRITTENDATE:
			case OTBM_ATTR_SLEEPERGUID:
			case OTBM_ATTR_SLEEPSTART: {
				// Known fixed-width attributes we do not surface yet.
				if (attribute == OTBM_ATTR_DURATION || attribute == OTBM_ATTR_WRITTENDATE || attribute == OTBM_ATTR_SLEEPERGUID || attribute == OTBM_ATTR_SLEEPSTART) {
					uint32_t skip = 0;
					if (!node->getU32(skip)) {
						return false;
					}
				} else {
					uint8_t skip = 0;
					if (!node->getU8(skip)) {
						return false;
					}
				}
				break;
			}
			case OTBM_ATTR_ATTRIBUTE_MAP: {
				uint16_t size = 0;
				if (!node->getU16(size) || !node->skip(size)) {
					return false;
				}
				break;
			}
			default:
				// Unknown attribute: stop attribute parsing; remaining bytes may be unused.
				return true;
		}
	}

	if (BinaryNode* child = node->getChild()) {
		do {
			uint8_t type = 0;
			if (!child->getU8(type) || type != OTBM_ITEM) {
				continue;
			}
			Item nested;
			if (LoadItem(child, nested)) {
				item.getContents().push_back(std::move(nested));
			}
		} while (child->advance());
	}
	return true;
}

void SaveItem(NodeFileWriteHandle& handle, const Item& item) {
	handle.addNode(OTBM_ITEM);
	handle.addU16(item.getID());
	if (item.getCount() != 1) {
		handle.addU8(OTBM_ATTR_COUNT);
		handle.addU8(item.getCount());
	}
	if (item.getActionID() != 0) {
		handle.addU8(OTBM_ATTR_ACTION_ID);
		handle.addU16(item.getActionID());
	}
	if (item.getUniqueID() != 0) {
		handle.addU8(OTBM_ATTR_UNIQUE_ID);
		handle.addU16(item.getUniqueID());
	}
	if (item.getCharges() != 0) {
		handle.addU8(OTBM_ATTR_CHARGES);
		handle.addU16(item.getCharges());
	}
	if (!item.getText().empty()) {
		handle.addU8(OTBM_ATTR_TEXT);
		handle.addString(item.getText());
	}
	if (!item.getDescription().empty()) {
		handle.addU8(OTBM_ATTR_DESC);
		handle.addString(item.getDescription());
	}
	if (item.hasDestination()) {
		handle.addU8(OTBM_ATTR_TELE_DEST);
		handle.addU16(static_cast<uint16_t>(item.getDestination().x));
		handle.addU16(static_cast<uint16_t>(item.getDestination().y));
		handle.addU8(static_cast<uint8_t>(item.getDestination().z));
	}
	if (item.getDepotID() != 0) {
		handle.addU8(OTBM_ATTR_DEPOT_ID);
		handle.addU16(item.getDepotID());
	}
	if (item.getDoorID() != 0) {
		handle.addU8(OTBM_ATTR_HOUSEDOORID);
		handle.addU8(item.getDoorID());
	}
	for (const Item& nested : item.getContents()) {
		SaveItem(handle, nested);
	}
	handle.endNode();
}

bool LoadTile(BinaryNode* node, Map& map, int base_x, int base_y, int z, bool house_tile) {
	uint8_t dx = 0;
	uint8_t dy = 0;
	if (!node->getU8(dx) || !node->getU8(dy)) {
		return false;
	}

	Position position(base_x + dx, base_y + dy, z);
	Tile tile(position);
	if (house_tile) {
		uint32_t house_id = 0;
		if (!node->getU32(house_id)) {
			return false;
		}
		tile.setHouseID(house_id);
	}

	uint8_t attribute = 0;
	while (node->getU8(attribute)) {
		if (attribute == OTBM_ATTR_TILE_FLAGS) {
			uint32_t flags = 0;
			if (!node->getU32(flags)) {
				return false;
			}
			tile.setFlags(flags);
		} else if (attribute == OTBM_ATTR_ITEM) {
			Item item;
			uint16_t id = 0;
			if (!node->getU16(id)) {
				return false;
			}
			item.setID(id);
			if (!tile.hasGround()) {
				tile.setGround(std::move(item));
			} else {
				tile.addItem(std::move(item));
			}
		} else {
			break;
		}
	}

	if (BinaryNode* child = node->getChild()) {
		do {
			uint8_t type = 0;
			if (!child->getU8(type)) {
				continue;
			}
			if (type == OTBM_ITEM) {
				Item item;
				if (!LoadItem(child, item)) {
					continue;
				}
				if (!tile.hasGround()) {
					tile.setGround(std::move(item));
				} else {
					tile.addItem(std::move(item));
				}
			}
		} while (child->advance());
	}

	if (!tile.empty()) {
		map.setTile(std::move(tile));
	}
	return true;
}

} // namespace

bool LoadOTBM(Map& map, const std::string& path) {
	map.clearMessages();
	DiskNodeFileReadHandle handle(path, {"OTBM"});
	if (!handle.isOk()) {
		map.setError(handle.getErrorMessage());
		return false;
	}

	BinaryNode* root = handle.getRootNode();
	if (!root) {
		map.setError("OTBM root node missing");
		return false;
	}

	uint8_t root_type = 0;
	if (!root->getU8(root_type) || root_type != OTBM_ROOTV1) {
		map.setError("Unsupported OTBM root type");
		return false;
	}

	MapVersion version;
	uint16_t width = 0;
	uint16_t height = 0;
	if (!root->getU32(version.otbm) || !root->getU16(width) || !root->getU16(height)
		|| !root->getU32(version.items_major) || !root->getU32(version.items_minor)) {
		map.setError("Failed to read OTBM header");
		return false;
	}

	map.clear();
	map.setSize(width, height);
	map.setVersion(version);
	map.setName(std::filesystem::path(path).filename().string());

	BinaryNode* map_data = root->getChild();
	if (!map_data) {
		map.setError("OTBM map data node missing");
		return false;
	}

	do {
		uint8_t type = 0;
		if (!map_data->getU8(type)) {
			continue;
		}
		if (type != OTBM_MAP_DATA) {
			map.addWarning("Skipping unexpected node type " + i2s(type));
			continue;
		}

		uint8_t attribute = 0;
		while (map_data->getU8(attribute)) {
			std::string value;
			switch (attribute) {
				case OTBM_ATTR_DESCRIPTION:
					if (map_data->getString(value)) {
						map.setDescription(value);
					}
					break;
				case OTBM_ATTR_EXT_FILE:
				case OTBM_ATTR_EXT_SPAWN_MONSTER_FILE:
					if (map_data->getString(value)) {
						map.setSpawnFilename(value);
					}
					break;
				case OTBM_ATTR_EXT_HOUSE_FILE:
					if (map_data->getString(value)) {
						map.setHouseFilename(value);
					}
					break;
				case OTBM_ATTR_EXT_SPAWN_NPC_FILE:
					if (map_data->getString(value)) {
						map.setSpawnNpcFilename(value);
					}
					break;
				case OTBM_ATTR_EXT_ZONE_FILE:
					if (map_data->getString(value)) {
						map.setZoneFilename(value);
					}
					break;
				default:
					attribute = 0;
					break;
			}
			if (attribute == 0) {
				break;
			}
		}

		if (BinaryNode* child = map_data->getChild()) {
			do {
				uint8_t child_type = 0;
				if (!child->getU8(child_type)) {
					continue;
				}
				if (child_type == OTBM_TILE_AREA) {
					uint16_t area_x = 0, area_y = 0;
					uint8_t area_z = 0;
					if (!child->getU16(area_x) || !child->getU16(area_y) || !child->getU8(area_z)) {
						continue;
					}
					if (BinaryNode* tile_node = child->getChild()) {
						do {
							uint8_t tile_type = 0;
							if (!tile_node->getU8(tile_type)) {
								continue;
							}
							if (tile_type == OTBM_TILE || tile_type == OTBM_HOUSETILE) {
								LoadTile(tile_node, map, area_x, area_y, area_z, tile_type == OTBM_HOUSETILE);
							}
						} while (tile_node->advance());
					}
				} else if (child_type == OTBM_TOWNS) {
					if (BinaryNode* town_node = child->getChild()) {
						do {
							uint8_t town_type = 0;
							if (!town_node->getU8(town_type) || town_type != OTBM_TOWN) {
								continue;
							}
							Town town;
							uint16_t x = 0, y = 0;
							uint8_t z = 0;
							if (!town_node->getU32(town.id) || !town_node->getString(town.name)
								|| !town_node->getU16(x) || !town_node->getU16(y) || !town_node->getU8(z)) {
								continue;
							}
							town.temple = Position(x, y, z);
							map.towns().push_back(std::move(town));
						} while (town_node->advance());
					}
				} else if (child_type == OTBM_WAYPOINTS) {
					if (BinaryNode* wp_node = child->getChild()) {
						do {
							uint8_t wp_type = 0;
							if (!wp_node->getU8(wp_type) || wp_type != OTBM_WAYPOINT) {
								continue;
							}
							Waypoint waypoint;
							uint16_t x = 0, y = 0;
							uint8_t z = 0;
							if (!wp_node->getString(waypoint.name) || !wp_node->getU16(x) || !wp_node->getU16(y) || !wp_node->getU8(z)) {
								continue;
							}
							waypoint.position = Position(x, y, z);
							map.waypoints().push_back(std::move(waypoint));
						} while (wp_node->advance());
					}
				}
			} while (child->advance());
		}
	} while (map_data->advance());

	map.clearChanges();
	return true;
}

bool SaveOTBM(const Map& map, const std::string& path) {
	DiskNodeFileWriteHandle handle(path, "OTBM");
	if (!handle.isOk()) {
		return false;
	}

	handle.addNode(OTBM_ROOTV1);
	handle.addU32(map.getVersion().otbm);
	handle.addU16(static_cast<uint16_t>(map.getWidth()));
	handle.addU16(static_cast<uint16_t>(map.getHeight()));
	handle.addU32(map.getVersion().items_major);
	handle.addU32(map.getVersion().items_minor);

	handle.addNode(OTBM_MAP_DATA);
	handle.addU8(OTBM_ATTR_DESCRIPTION);
	handle.addString(map.getDescription());
	if (!map.getSpawnFilename().empty()) {
		handle.addU8(OTBM_ATTR_EXT_SPAWN_MONSTER_FILE);
		handle.addString(map.getSpawnFilename());
	}
	if (!map.getHouseFilename().empty()) {
		handle.addU8(OTBM_ATTR_EXT_HOUSE_FILE);
		handle.addString(map.getHouseFilename());
	}
	if (!map.getSpawnNpcFilename().empty()) {
		handle.addU8(OTBM_ATTR_EXT_SPAWN_NPC_FILE);
		handle.addString(map.getSpawnNpcFilename());
	}
	if (!map.getZoneFilename().empty()) {
		handle.addU8(OTBM_ATTR_EXT_ZONE_FILE);
		handle.addString(map.getZoneFilename());
	}

	// Group tiles into 256x256 areas per floor, matching classic OTBM writers.
	struct AreaKey {
		int x;
		int y;
		int z;
		bool operator==(const AreaKey& other) const { return x == other.x && y == other.y && z == other.z; }
	};
	struct AreaHash {
		std::size_t operator()(const AreaKey& key) const {
			return (static_cast<std::size_t>(key.x) << 20) ^ (static_cast<std::size_t>(key.y) << 8) ^ static_cast<std::size_t>(key.z);
		}
	};
	std::unordered_map<AreaKey, std::vector<const Tile*>, AreaHash> areas;
	for (const auto& [_, tile] : map.tiles()) {
		const Position& pos = tile.getPosition();
		areas[AreaKey{pos.x & ~0xFF, pos.y & ~0xFF, pos.z}].push_back(&tile);
	}

	for (const auto& [area, tiles] : areas) {
		handle.addNode(OTBM_TILE_AREA);
		handle.addU16(static_cast<uint16_t>(area.x));
		handle.addU16(static_cast<uint16_t>(area.y));
		handle.addU8(static_cast<uint8_t>(area.z));
		for (const Tile* tile : tiles) {
			const bool house = tile->getHouseID() != 0;
			handle.addNode(house ? OTBM_HOUSETILE : OTBM_TILE);
			handle.addU8(static_cast<uint8_t>(tile->getPosition().x - area.x));
			handle.addU8(static_cast<uint8_t>(tile->getPosition().y - area.y));
			if (house) {
				handle.addU32(tile->getHouseID());
			}
			if (tile->getFlags() != 0) {
				handle.addU8(OTBM_ATTR_TILE_FLAGS);
				handle.addU32(tile->getFlags());
			}
			if (const Item* ground = tile->getGround()) {
				SaveItem(handle, *ground);
			}
			for (const Item& item : tile->getItems()) {
				SaveItem(handle, item);
			}
			handle.endNode();
		}
		handle.endNode();
	}

	handle.addNode(OTBM_TOWNS);
	for (const Town& town : map.towns()) {
		handle.addNode(OTBM_TOWN);
		handle.addU32(town.id);
		handle.addString(town.name);
		handle.addU16(static_cast<uint16_t>(town.temple.x));
		handle.addU16(static_cast<uint16_t>(town.temple.y));
		handle.addU8(static_cast<uint8_t>(town.temple.z));
		handle.endNode();
	}
	handle.endNode();

	handle.addNode(OTBM_WAYPOINTS);
	for (const Waypoint& waypoint : map.waypoints()) {
		handle.addNode(OTBM_WAYPOINT);
		handle.addString(waypoint.name);
		handle.addU16(static_cast<uint16_t>(waypoint.position.x));
		handle.addU16(static_cast<uint16_t>(waypoint.position.y));
		handle.addU8(static_cast<uint8_t>(waypoint.position.z));
		handle.endNode();
	}
	handle.endNode();

	handle.endNode(); // MAP_DATA
	handle.endNode(); // ROOT
	handle.close();
	return true;
}

bool WriteSampleOTBM(const std::string& path) {
	Map map;
	map.createEmpty(256, 256, "sample.otbm");
	map.setDescription("RME Wasm Phase 9 sample map (tilesets, wall auto-connect)");
	map.setHouseFilename("houses.xml");
	map.setSpawnFilename("spawn.xml");

	Town temple;
	temple.id = 1;
	temple.name = "Sample";
	temple.temple = Position(100, 100, rme::MapGroundLayer);
	map.towns().push_back(temple);

	Waypoint waypoint;
	waypoint.name = "center";
	waypoint.position = Position(100, 100, rme::MapGroundLayer);
	map.waypoints().push_back(waypoint);

	House cabin;
	cabin.id = 1;
	cabin.name = "Sample Cabin";
	cabin.town_id = 1;
	cabin.entry = Position(102, 102, rme::MapGroundLayer);
	cabin.rent = 500;
	map.houses().push_back(cabin);

	Spawn spawn;
	spawn.center = Position(100, 100, rme::MapGroundLayer);
	spawn.radius = 3;
	spawn.monsters.push_back(SpawnCreature{"Rat", 0, 0, 60});
	spawn.monsters.push_back(SpawnCreature{"Cave Rat", 1, 1, 90});
	map.spawns().push_back(std::move(spawn));

	for (int y = 98; y <= 102; ++y) {
		for (int x = 98; x <= 102; ++x) {
			Tile tile(Position(x, y, rme::MapGroundLayer));
			uint16_t ground_id = 100;
			if (x >= 99 && x <= 101 && y >= 99 && y <= 101) {
				ground_id = 102;
			} else if (x == 98 || y == 98) {
				ground_id = 101;
			}
			tile.setGround(Item(ground_id));
			if (ground_id == 102) {
				tile.setFlag(TILESTATE_PROTECTIONZONE);
			}
			if (x == 102) {
				tile.setHouseID(1);
			}
			if (x == 100 && y == 100) {
				Item crate(105);
				crate.setActionID(1000);
				crate.setUniqueID(2000);
				crate.setText("Phase 8 crate");
				Item loot(104);
				loot.setCount(3);
				loot.setText("Phase 8 loot");
				crate.getContents().push_back(std::move(loot));
				tile.addItem(std::move(crate));
				Item cover(104);
				cover.setActionID(8);
				tile.addItem(std::move(cover));
			} else if (x == 100 && y == 98) {
				tile.addItem(Item(107));
			} else if (x == 101 && y == 98) {
				tile.addItem(Item(109));
			} else if (x == 101 && y == 99) {
				tile.addItem(Item(108));
			} else if (x == 99 && y == 102) {
				Item portal(104);
				portal.setActionID(7);
				portal.setDestination(Position(100, 100, rme::MapGroundLayer + 1));
				tile.addItem(std::move(portal));
			} else if (x == 98 && y == 100) {
				Item broken(104);
				broken.setActionID(9);
				broken.setDestination(Position(10, 10, rme::MapGroundLayer));
				tile.addItem(std::move(broken));
			} else if (x == 102 && y == 102) {
				Item extra(104);
				extra.setUniqueID(2000);
				tile.addItem(std::move(extra));
			}
			map.setTile(std::move(tile));
		}
	}

	for (int y = 99; y <= 101; ++y) {
		for (int x = 99; x <= 101; ++x) {
			Tile cave(Position(x, y, rme::MapGroundLayer + 1));
			cave.setGround(Item(101));
			if (x == 100 && y == 100) {
				Item wall(103);
				wall.setDoorID(1);
				cave.addItem(std::move(wall));
			}
			map.setTile(std::move(cave));
		}
	}

	if (!SaveOTBM(map, path)) {
		return false;
	}
	const std::string houses = CompanionPath(path, map.getHouseFilename(), "houses.xml");
	const std::string spawns = CompanionPath(path, map.getSpawnFilename(), "spawn.xml");
	return SaveHouseXml(map, houses) && SaveSpawnXml(map, spawns);
}

} // namespace core
} // namespace rme
