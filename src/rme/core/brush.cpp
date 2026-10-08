#include "brush.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace rme {
namespace core {

const char* BrushKindName(BrushKind kind) {
	switch (kind) {
		case BrushKind::Auto:
			return "Auto";
		case BrushKind::Ground:
			return "Ground";
		case BrushKind::Overlay:
			return "Item";
		case BrushKind::Eraser:
			return "Eraser";
		case BrushKind::Fill:
			return "Fill";
		case BrushKind::Select:
			return "Select";
		case BrushKind::Flags:
			return "Flags";
		case BrushKind::House:
			return "House";
		case BrushKind::Wall:
			return "Wall";
		default:
			return "Brush";
	}
}

BrushKind ResolveBrush(BrushKind kind, const ItemType* type) {
	if (kind != BrushKind::Auto) {
		return kind;
	}
	if (type && !type->ground) {
		return BrushKind::Overlay;
	}
	return BrushKind::Ground;
}

std::vector<Position> BrushFootprint(const Position& center, int size, int map_width, int map_height) {
	std::vector<Position> cells;
	const int radius = std::max(0, (size - 1) / 2);
	cells.reserve(static_cast<std::size_t>((radius * 2 + 1) * (radius * 2 + 1)));
	for (int dy = -radius; dy <= radius; ++dy) {
		for (int dx = -radius; dx <= radius; ++dx) {
			const Position next(center.x + dx, center.y + dy, center.z);
			if (next.x >= 0 && next.y >= 0 && next.x < map_width && next.y < map_height) {
				cells.push_back(next);
			}
		}
	}
	return cells;
}

std::vector<Position> SelectionTiles(const Position& a, const Position& b) {
	std::vector<Position> cells;
	const int x0 = std::min(a.x, b.x);
	const int x1 = std::max(a.x, b.x);
	const int y0 = std::min(a.y, b.y);
	const int y1 = std::max(a.y, b.y);
	const int z = a.z;
	cells.reserve(static_cast<std::size_t>((x1 - x0 + 1) * (y1 - y0 + 1)));
	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			cells.emplace_back(x, y, z);
		}
	}
	return cells;
}

std::vector<Position> FloodGround(const Map& map, const Position& start) {
	std::vector<Position> cells;
	if (!map.inBounds(start)) {
		return cells;
	}
	const Tile* seed = map.getTile(start);
	if (!seed || !seed->hasGround()) {
		return cells;
	}
	const uint16_t id = seed->getGround()->getID();

	std::queue<Position> pending;
	std::unordered_set<uint64_t> seen;
	pending.push(start);
	seen.insert(MakeTileKey(start.x, start.y, start.z));

	constexpr int kDx[4] = {1, -1, 0, 0};
	constexpr int kDy[4] = {0, 0, 1, -1};
	while (!pending.empty()) {
		const Position current = pending.front();
		pending.pop();
		cells.push_back(current);
		for (int i = 0; i < 4; ++i) {
			const Position next(current.x + kDx[i], current.y + kDy[i], current.z);
			if (!map.inBounds(next)) {
				continue;
			}
			const uint64_t key = MakeTileKey(next.x, next.y, next.z);
			if (seen.count(key)) {
				continue;
			}
			const Tile* tile = map.getTile(next);
			if (!tile || !tile->hasGround() || tile->getGround()->getID() != id) {
				continue;
			}
			seen.insert(key);
			pending.push(next);
		}
	}
	return cells;
}

bool ApplyBrushToTile(Tile& tile, BrushKind resolved, uint16_t item_id, uint32_t flag_mask, bool clear_flag, uint32_t house_id) {
	switch (resolved) {
		case BrushKind::Ground:
			if (tile.hasGround() && tile.getGround()->getID() == item_id) {
				return false;
			}
			tile.setGround(Item(item_id));
			return true;
		case BrushKind::Overlay:
			for (const Item& item : tile.getItems()) {
				if (item.getID() == item_id) {
					return false;
				}
			}
			tile.addItem(Item(item_id));
			return true;
		case BrushKind::Eraser:
			if (tile.popTopItem()) {
				return true;
			}
			if (tile.hasGround()) {
				tile.clearGround();
				return true;
			}
			if (tile.getFlags() != 0) {
				tile.setFlags(0);
				return true;
			}
			if (tile.getHouseID() != 0) {
				tile.setHouseID(0);
				return true;
			}
			return false;
		case BrushKind::Flags: {
			const uint32_t mask = flag_mask == 0 ? TILESTATE_PROTECTIONZONE : flag_mask;
			const uint32_t next = clear_flag ? (tile.getFlags() & ~mask) : (tile.getFlags() | mask);
			if (next == tile.getFlags()) {
				return false;
			}
			tile.setFlags(next);
			return true;
		}
		case BrushKind::House: {
			const uint32_t next = clear_flag ? 0 : house_id;
			if (tile.getHouseID() == next) {
				return false;
			}
			tile.setHouseID(next);
			return true;
		}
		default:
			return false;
	}
}

} // namespace core
} // namespace rme
