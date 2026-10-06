#pragma once

#include "action.h"
#include "brush.h"
#include "items_dat.h"
#include "minimap.h"
#include "otbm.h"
#include "sprites.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace rme {
namespace core {

class EditorSession {
public:
	EditorSession();

	Map& map() { return map_; }
	const Map& map() const { return map_; }
	const ClientAssetsInfo& assets() const { return assets_; }
	ItemDatabase& items() { return items_; }
	const ItemDatabase& items() const { return items_; }
	SpriteSheet& sprites() { return sprites_; }
	const SpriteSheet& sprites() const { return sprites_; }
	ActionQueue& history() { return history_; }
	Selection& selection() { return selection_; }
	const Selection& selection() const { return selection_; }
	const Minimap& minimap();

	void newMap(int width = 256, int height = 256);
	bool loadOtbm(const std::string& path);
	bool saveOtbm(const std::string& path);
	bool loadDat(const std::string& path);
	bool loadSpr(const std::string& path);
	bool loadHouseXml(const std::string& path);
	bool loadSpawnXml(const std::string& path);
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
	void setBrushId(uint16_t id) { brush_id_ = id; }
	void setBrushSize(int size);
	void setBrushKind(BrushKind kind) { brush_kind_ = kind; }
	void setFlagMask(uint32_t mask) { flag_mask_ = mask == 0 ? TILESTATE_PROTECTIONZONE : mask; }
	uint32_t flagMask() const { return flag_mask_; }
	void setHouseId(uint32_t id) { house_id_ = id; }
	uint32_t houseId() const { return house_id_; }
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
	bool removeSpawn(std::size_t index);
	bool goToSpawn(std::size_t index);
	bool addSpawnMonster(std::size_t spawn_index, std::string name, int dx = 0, int dy = 0, uint32_t spawntime = 60);

	const ItemType* brushType() const { return items_.get(brush_id_); }
	uint16_t spriteIdForItem(uint16_t item_id) const;
	std::vector<Position> hoverFootprint(const Position& center) const;
	const std::string& lastError() const { return last_error_; }

private:
	void cancelStroke();
	void markMinimapDirty() { minimap_dirty_ = true; }
	bool applyLive(const Position& position, BrushKind kind, uint16_t item_id, Action& action, bool invert = false);

	Map map_;
	ActionQueue history_;
	ClientAssetsInfo assets_;
	ItemDatabase items_;
	SpriteSheet sprites_;
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
	std::string last_error_;
};

} // namespace core
} // namespace rme
