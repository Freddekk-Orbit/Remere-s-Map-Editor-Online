#pragma once

#include "action.h"
#include "brush.h"
#include "items_dat.h"
#include "materials.h"
#include "minimap.h"
#include "otbm.h"
#include "sprites.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace rme {
namespace core {

struct ItemProps {
	uint16_t action_id = 0;
	uint16_t unique_id = 0;
	uint8_t count = 1;
	uint16_t charges = 0;
	uint16_t depot_id = 0;
	uint8_t door_id = 0;
	bool has_destination = false;
	Position destination;
	std::string text;
	std::string description;
};

struct FindQuery {
	uint16_t item_id = 0;
	uint16_t action_id = 0;
	uint16_t unique_id = 0;
	bool teleports_only = false;

	bool empty() const {
		return item_id == 0 && action_id == 0 && unique_id == 0 && !teleports_only;
	}
};

struct StackEntry {
	int index = 0;
	uint16_t item_id = 0;
	bool is_ground = false;
	std::size_t content_count = 0;
	uint16_t action_id = 0;
	uint16_t unique_id = 0;
};

enum class MapIssueKind {
	DuplicateUniqueId,
	InvalidTeleport,
	UnknownItem,
};

struct MapIssue {
	MapIssueKind kind = MapIssueKind::UnknownItem;
	Position position;
	uint16_t item_id = 0;
	uint16_t unique_id = 0;
	std::string message;
};

inline const char* MapIssueKindName(MapIssueKind kind) {
	switch (kind) {
		case MapIssueKind::DuplicateUniqueId:
			return "Duplicate UID";
		case MapIssueKind::InvalidTeleport:
			return "Bad teleport";
		case MapIssueKind::UnknownItem:
			return "Unknown item";
	}
	return "Issue";
}

class EditorSession {
public:
	EditorSession();

	Map& map() { return map_; }
	const Map& map() const { return map_; }
	const ClientAssetsInfo& assets() const { return assets_; }
	ItemDatabase& items() { return items_; }
	const ItemDatabase& items() const { return items_; }
	Materials& materials() { return materials_; }
	const Materials& materials() const { return materials_; }
	SpriteSheet& sprites() { return sprites_; }
	const SpriteSheet& sprites() const { return sprites_; }
	ActionQueue& history() { return history_; }
	Selection& selection() { return selection_; }
	const Selection& selection() const { return selection_; }
	const Minimap& minimap();

	void newMap(int width = 256, int height = 256, std::string name = "Untitled.otbm");
	bool loadOtbm(const std::string& path);
	bool saveOtbm(const std::string& path);
	bool saveMapZip(const std::string& path);
	void setMapName(std::string name);
	void setMapDescription(std::string description);
	std::string otbmFileName() const;
	std::string zipFileName() const;
	bool loadDat(const std::string& path);
	bool loadSpr(const std::string& path);
	bool loadHouseXml(const std::string& path);
	bool loadSpawnXml(const std::string& path);
	bool loadMaterials(const std::string& path);
	bool createSampleMap(const std::string& path);
	bool createSampleAssets(const std::string& dat_path, const std::string& spr_path);

	void paintGround(const Position& position, uint16_t item_id);
	void eraseTile(const Position& position);

	void beginStroke();
	void strokeAt(const Position& position, bool invert = false);
	void endStroke();
	bool isStroking() const { return stroking_; }

	void fillAt(const Position& position);
	bool pickAt(const Position& position);
	void deleteSelection();
	void copySelection();
	void cutSelection();
	void pasteAt(const Position& position);
	bool hasClipboard() const { return !clipboard_.empty(); }
	std::size_t clipboardSize() const { return clipboard_.size(); }

	bool canUndo() const { return history_.canUndo(); }
	bool canRedo() const { return history_.canRedo(); }
	bool undo();
	bool redo();

	int cameraX() const { return camera_x_; }
	int cameraY() const { return camera_y_; }
	int floor() const { return floor_; }
	uint16_t brushId() const { return brush_id_; }
	int brushSize() const { return brush_size_; }
	BrushKind brushKind() const { return brush_kind_; }
	BrushKind resolvedBrush() const;
	void setCamera(int x, int y);
	void panBy(int dx, int dy);
	void setFloor(int floor);
	void setBrushId(uint16_t id);
	void setBrushSize(int size);
	void setBrushKind(BrushKind kind) { brush_kind_ = kind; }
	void setFlagMask(uint32_t mask) { flag_mask_ = mask == 0 ? TILESTATE_PROTECTIONZONE : mask; }
	uint32_t flagMask() const { return flag_mask_; }
	void setHouseId(uint32_t id) { house_id_ = id; }
	uint32_t houseId() const { return house_id_; }
	void setCreatureName(std::string name);
	const std::string& creatureName() const { return creature_name_; }
	void setSpawnRadius(int radius);
	int spawnRadius() const { return spawn_radius_; }
	void setSpawnTime(uint32_t seconds);
	uint32_t spawnTime() const { return spawn_time_; }
	int findSpawnCovering(const Position& position) const;
	std::vector<std::string> creaturesAt(const Position& position) const;
	void goTo(int x, int y, int z);
	void centerOnOccupied();

	uint32_t addTown(std::string name, const Position& temple);
	bool removeTown(uint32_t id);
	bool renameTown(uint32_t id, std::string name);
	bool goToTown(uint32_t id);
	void addWaypoint(std::string name, const Position& position);
	bool removeWaypoint(std::size_t index);
	bool goToWaypoint(std::size_t index);

	uint32_t addHouse(std::string name, const Position& entry);
	bool removeHouse(uint32_t id);
	bool renameHouse(uint32_t id, std::string name);
	bool goToHouse(uint32_t id);
	std::size_t addSpawn(const Position& center, int radius = 3);
	std::size_t addSpawn(const Position& center, int radius, std::string first_monster);
	bool removeSpawn(std::size_t index);
	bool goToSpawn(std::size_t index);
	bool addSpawnMonster(std::size_t spawn_index, std::string name, int dx = 0, int dy = 0, uint32_t spawntime = 60);

	const ItemType* brushType() const { return items_.get(brush_id_); }
	uint16_t spriteIdForItem(uint16_t item_id) const;
	std::vector<Position> hoverFootprint(const Position& center) const;
	const std::string& lastError() const { return last_error_; }

	void setInspect(const Position& position);
	Position inspect() const { return inspect_; }
	int inspectIndex() const;
	void setInspectIndex(int index);
	const Item* inspectItem() const;
	Item* inspectItem();
	std::vector<StackEntry> browseInspect() const;
	bool editTopItem(const ItemProps& props);
	bool removeInspectItem();
	bool moveInspectItem(int delta);
	bool addContainerItem(uint16_t item_id, uint8_t count = 1);
	bool removeContainerItem(std::size_t index);
	std::vector<Position> findTiles(const FindQuery& query) const;
	bool findNext(const FindQuery& query);
	std::vector<MapIssue> mapIssues() const;
	bool goToIssue(std::size_t index);

private:
	void cancelStroke();
	void markMinimapDirty() { minimap_dirty_ = true; }
	bool applyLive(const Position& position, BrushKind kind, uint16_t item_id, Action& action, bool invert = false);
	void clampInspectIndex();
	bool recordInspectTile(Tile after);
	bool writeTileAfter(Action& action, const Position& position, Tile after);
	void strokeWallAt(const Position& position, bool invert);
	void strokeBorderAt(const Position& position, bool invert);
	void restitchBorders(Action& action, const GroundBorderSet& set, const std::vector<Position>& seeds);
	void strokeDoodadAt(const Position& position, bool invert);
	void strokeDoorAt(const Position& position, bool invert);
	void strokeTableAt(const Position& position, bool invert);
	void strokeCarpetAt(const Position& position, bool invert);
	void strokeCreatureAt(const Position& position, bool invert);
	void removeCreaturesAt(const Position& position);

	Map map_;
	ActionQueue history_;
	ClientAssetsInfo assets_;
	ItemDatabase items_;
	SpriteSheet sprites_;
	Materials materials_;
	Minimap minimap_;
	bool minimap_dirty_ = true;
	Selection selection_;
	std::vector<ClipboardTile> clipboard_;
	Action stroke_{ActionIdentifier::BrushStroke};
	std::unordered_set<uint64_t> stroke_seen_;
	bool stroking_ = false;
	int camera_x_ = 100;
	int camera_y_ = 100;
	int floor_ = rme::MapGroundLayer;
	uint16_t brush_id_ = 100;
	int brush_size_ = 1;
	BrushKind brush_kind_ = BrushKind::Auto;
	uint32_t flag_mask_ = TILESTATE_PROTECTIONZONE;
	uint32_t house_id_ = 1;
	int spawn_radius_ = 3;
	uint32_t spawn_time_ = 60;
	std::string creature_name_ = "Rat";
	int stroke_spawn_index_ = -1;
	Position inspect_{100, 100, rme::MapGroundLayer};
	int inspect_index_ = -1;
	std::string last_error_;
};

inline ItemProps PropsFromItem(const Item& item) {
	ItemProps props;
	props.action_id = item.getActionID();
	props.unique_id = item.getUniqueID();
	props.count = item.getCount();
	props.charges = item.getCharges();
	props.depot_id = item.getDepotID();
	props.door_id = item.getDoorID();
	props.has_destination = item.hasDestination();
	props.destination = item.getDestination();
	props.text = item.getText();
	props.description = item.getDescription();
	return props;
}

inline bool PropsEqual(const ItemProps& a, const ItemProps& b) {
	return a.action_id == b.action_id && a.unique_id == b.unique_id && a.count == b.count
		&& a.charges == b.charges && a.depot_id == b.depot_id && a.door_id == b.door_id
		&& a.has_destination == b.has_destination
		&& (!a.has_destination || a.destination == b.destination) && a.text == b.text
		&& a.description == b.description;
}

inline void ApplyPropsToItem(Item& item, const ItemProps& props) {
	item.setActionID(props.action_id);
	item.setUniqueID(props.unique_id);
	item.setCount(props.count == 0 ? 1 : props.count);
	item.setCharges(props.charges);
	item.setDepotID(props.depot_id);
	item.setDoorID(props.door_id);
	item.setText(props.text);
	item.setDescription(props.description);
	if (props.has_destination) {
		item.setDestination(props.destination);
	} else {
		item.clearDestination();
	}
}

inline bool ItemMatchesQuery(const Item& item, const FindQuery& query) {
	if (query.empty()) {
		return false;
	}
	if (query.item_id != 0 && item.getID() != query.item_id) {
		return false;
	}
	if (query.action_id != 0 && item.getActionID() != query.action_id) {
		return false;
	}
	if (query.unique_id != 0 && item.getUniqueID() != query.unique_id) {
		return false;
	}
	if (query.teleports_only && !item.hasDestination()) {
		return false;
	}
	return true;
}

inline bool ItemMatchesQueryDeep(const Item& item, const FindQuery& query) {
	if (ItemMatchesQuery(item, query)) {
		return true;
	}
	for (const Item& nested : item.getContents()) {
		if (ItemMatchesQueryDeep(nested, query)) {
			return true;
		}
	}
	return false;
}

} // namespace core
} // namespace rme
