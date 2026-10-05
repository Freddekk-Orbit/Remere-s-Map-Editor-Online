#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rme {
namespace core {

// Classic Tibia.dat attribute ids (7.40–10.56 stream, terminated by 0xFF).
enum DatAttr : uint8_t {
	DatAttrGround = 0,
	DatAttrGroundBorder = 1,
	DatAttrOnBottom = 2,
	DatAttrOnTop = 3,
	DatAttrContainer = 4,
	DatAttrStackable = 5,
	DatAttrForceUse = 6,
	DatAttrMultiUse = 7,
	DatAttrWritable = 8,
	DatAttrWritableOnce = 9,
	DatAttrFluidContainer = 10,
	DatAttrSplash = 11,
	DatAttrNotWalkable = 12,
	DatAttrNotMoveable = 13,
	DatAttrBlockProjectile = 14,
	DatAttrNotPathable = 15,
	DatAttrPickupable = 16,
	DatAttrHangable = 17,
	DatAttrHookSouth = 18,
	DatAttrHookEast = 19,
	DatAttrRotateable = 20,
	DatAttrLight = 21,
	DatAttrDontHide = 22,
	DatAttrTranslucent = 23,
	DatAttrDisplacement = 24,
	DatAttrElevation = 25,
	DatAttrLyingCorpse = 26,
	DatAttrAnimateAlways = 27,
	DatAttrMinimapColor = 28,
	DatAttrLensHelp = 29,
	DatAttrFullGround = 30,
	DatAttrLook = 31,
	DatAttrCloth = 32,
	DatAttrMarket = 33,
	DatAttrUsable = 34,
	DatAttrLast = 255
};

struct ItemType {
	uint16_t id = 0;
	std::string name;
	bool ground = false;
	bool stackable = false;
	bool not_walkable = false;
	bool not_moveable = false;
	bool block_projectile = false;
	bool pickupable = false;
	bool container = false;
	bool full_ground = false;
	uint16_t ground_speed = 100;
	uint16_t minimap_color = 0;
	uint16_t sprite_id = 0;
	std::vector<uint16_t> sprite_ids;
	uint8_t width = 1;
	uint8_t height = 1;
	uint8_t layers = 1;
	uint8_t pattern_x = 1;
	uint8_t pattern_y = 1;
	uint8_t pattern_z = 1;
	uint8_t phases = 1;
};

} // namespace core
} // namespace rme
