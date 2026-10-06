#pragma once

#include "action.h"
#include "brush.h"
#include "items_dat.h"
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

	void newMap(int width = 256, int height = 256);
	bool loadOtbm(const std::string& path);
	bool saveOtbm(const std::string& path);
	bool loadDat(const std::string& path);
	bool loadSpr(const std::string& path);
	bool createSampleMap(const std::string& path);
	bool createSampleAssets(const std::string& dat_path, const std::string& spr_path);

	void paintGround(const Position& position, uint16_t item_id);
	void eraseTile(const Position& position);

	void beginStroke();
	void strokeAt(const Position& position);
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
	void setFloor(int floor);
	void setBrushId(uint16_t id) { brush_id_ = id; }
	void setBrushSize(int size);
	void setBrushKind(BrushKind kind) { brush_kind_ = kind; }
	void centerOnOccupied();

	const ItemType* brushType() const { return items_.get(brush_id_); }
	uint16_t spriteIdForItem(uint16_t item_id) const;
	std::vector<Position> hoverFootprint(const Position& center) const;
	const std::string& lastError() const { return last_error_; }

private:
	void cancelStroke();
	bool applyLive(const Position& position, BrushKind kind, uint16_t item_id, Action& action);

	Map map_;
	ActionQueue history_;
	ClientAssetsInfo assets_;
	ItemDatabase items_;
	SpriteSheet sprites_;
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
	std::string last_error_;
};

} // namespace core
} // namespace rme
