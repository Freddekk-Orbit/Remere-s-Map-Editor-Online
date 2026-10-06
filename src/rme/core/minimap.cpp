#include "minimap.h"

#include <algorithm>

namespace rme {
namespace core {

uint32_t MinimapRgba(uint16_t color_index) {
	const int color = static_cast<int>(color_index) % 216;
	const int r = (color / 36) % 6;
	const int g = (color / 6) % 6;
	const int b = color % 6;
	return (255u << 24) | (static_cast<uint32_t>(b * 51) << 16) | (static_cast<uint32_t>(g * 51) << 8) | static_cast<uint32_t>(r * 51);
}

uint32_t MinimapRgbaForTile(const Tile* tile, const ItemDatabase& items) {
	if (!tile) {
		return 0xFF101214;
	}
	uint16_t color = 0;
	bool found = false;
	if (!tile->getItems().empty()) {
		if (const ItemType* type = items.get(tile->getItems().back().getID()); type && type->minimap_color != 0) {
			color = type->minimap_color;
			found = true;
		}
	}
	if (!found && tile->hasGround()) {
		if (const ItemType* type = items.get(tile->getGround()->getID())) {
			color = type->minimap_color;
			found = type->minimap_color != 0;
		}
	}
	uint32_t rgba = found ? MinimapRgba(color) : 0xFF303438;
	if (tile->hasFlag(TILESTATE_PROTECTIONZONE)) {
		const uint32_t r = rgba & 0xFF;
		const uint32_t g = (rgba >> 8) & 0xFF;
		const uint32_t b = (rgba >> 16) & 0xFF;
		rgba = (255u << 24) | (std::min(255u, b) << 16) | (std::min(255u, g + 48) << 8) | (r * 3 / 4);
	}
	return rgba;
}

void Minimap::clear() {
	width_ = 0;
	height_ = 0;
	floor_ = 0;
	rgba_.clear();
}

void Minimap::rebuild(const Map& map, const ItemDatabase& items, int floor) {
	width_ = std::max(1, map.getWidth());
	height_ = std::max(1, map.getHeight());
	floor_ = floor;
	rgba_.assign(static_cast<std::size_t>(width_ * height_ * 4), 16);
	for (std::size_t i = 3; i < rgba_.size(); i += 4) {
		rgba_[i] = 255;
	}

	for (const auto& [_, tile] : map.tiles()) {
		const Position& pos = tile.getPosition();
		if (pos.z != floor || pos.x < 0 || pos.y < 0 || pos.x >= width_ || pos.y >= height_) {
			continue;
		}
		const uint32_t color = MinimapRgbaForTile(&tile, items);
		const int i = (pos.y * width_ + pos.x) * 4;
		rgba_[static_cast<std::size_t>(i) + 0] = static_cast<uint8_t>(color & 0xFF);
		rgba_[static_cast<std::size_t>(i) + 1] = static_cast<uint8_t>((color >> 8) & 0xFF);
		rgba_[static_cast<std::size_t>(i) + 2] = static_cast<uint8_t>((color >> 16) & 0xFF);
		rgba_[static_cast<std::size_t>(i) + 3] = 255;
	}
}

uint32_t Minimap::pixel(int x, int y) const {
	if (x < 0 || y < 0 || x >= width_ || y >= height_ || rgba_.empty()) {
		return 0;
	}
	const int i = (y * width_ + x) * 4;
	return (255u << 24)
		| (static_cast<uint32_t>(rgba_[static_cast<std::size_t>(i) + 2]) << 16)
		| (static_cast<uint32_t>(rgba_[static_cast<std::size_t>(i) + 1]) << 8)
		| rgba_[static_cast<std::size_t>(i) + 0];
}

} // namespace core
} // namespace rme
