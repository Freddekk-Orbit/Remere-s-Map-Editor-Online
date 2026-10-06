#pragma once

#include "items_dat.h"
#include "map.h"

#include <cstdint>
#include <vector>

namespace rme {
namespace core {

// Classic Tibia 6x6x6 minimap palette (color = r*36 + g*6 + b, channel 0..5 * 51).
uint32_t MinimapRgba(uint16_t color_index);
uint32_t MinimapRgbaForTile(const Tile* tile, const ItemDatabase& items);

class Minimap {
public:
	void clear();
	void rebuild(const Map& map, const ItemDatabase& items, int floor);

	int width() const { return width_; }
	int height() const { return height_; }
	int floor() const { return floor_; }
	const std::vector<uint8_t>& rgba() const { return rgba_; }
	bool empty() const { return rgba_.empty(); }

	uint32_t pixel(int x, int y) const;

private:
	int width_ = 0;
	int height_ = 0;
	int floor_ = 0;
	std::vector<uint8_t> rgba_;
};

} // namespace core
} // namespace rme
