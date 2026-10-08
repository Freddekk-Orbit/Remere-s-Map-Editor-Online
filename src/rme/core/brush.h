#pragma once

#include "item_type.h"
#include "map.h"

#include <cstdint>
#include <vector>

namespace rme {
namespace core {

enum class BrushKind {
	Auto,
	Ground,
	Overlay,
	Eraser,
	Fill,
	Select,
	Flags,
	House,
	Wall,
	Border,
};

const char* BrushKindName(BrushKind kind);

BrushKind ResolveBrush(BrushKind kind, const ItemType* type);

std::vector<Position> BrushFootprint(const Position& center, int size, int map_width, int map_height);

std::vector<Position> FloodGround(const Map& map, const Position& start);

std::vector<Position> SelectionTiles(const Position& a, const Position& b);

// Mutates tile. Returns true when the tile content changed.
bool ApplyBrushToTile(Tile& tile, BrushKind resolved, uint16_t item_id, uint32_t flag_mask = 0, bool clear_flag = false, uint32_t house_id = 0);

struct Selection {
	bool dragging = false;
	bool active = false;
	Position anchor;
	Position cursor;

	void clear() {
		dragging = false;
		active = false;
	}

	void begin(const Position& position) {
		anchor = cursor = position;
		dragging = true;
		active = false;
	}

	void update(const Position& position) {
		cursor = position;
	}

	void finish() {
		dragging = false;
		active = true;
	}

	bool visible() const { return dragging || active; }

	std::vector<Position> tiles() const {
		if (!visible()) {
			return {};
		}
		return SelectionTiles(anchor, cursor);
	}

	std::size_t size() const { return tiles().size(); }
};

struct ClipboardTile {
	int dx = 0;
	int dy = 0;
	Tile tile;
};

} // namespace core
} // namespace rme
