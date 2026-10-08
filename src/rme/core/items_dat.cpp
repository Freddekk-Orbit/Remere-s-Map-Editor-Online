#include "items_dat.h"

#include "filehandle.h"

#include <filesystem>

namespace rme {
namespace core {
namespace {

void ApplyBuiltinName(ItemType& type) {
	if (!type.name.empty()) {
		return;
	}
	switch (type.id) {
		case 100:
			type.name = "Grass";
			break;
		case 101:
			type.name = "Dirt";
			break;
		case 102:
			type.name = "Water";
			break;
		case 103:
			type.name = "Wall";
			break;
		case 104:
			type.name = "Flower";
			break;
		case 105:
			type.name = "Box";
			break;
		case 106:
			type.name = "Wall pole";
			break;
		case 107:
			type.name = "Wall h";
			break;
		case 108:
			type.name = "Wall v";
			break;
		case 109:
			type.name = "Wall join";
			break;
		default:
			type.name = "Item " + std::to_string(type.id);
			break;
	}
}

bool SkipAttrPayload(::FileReadHandle& file, uint8_t attr) {
	switch (attr) {
		case DatAttrGround:
		case DatAttrWritable:
		case DatAttrWritableOnce:
		case DatAttrElevation:
		case DatAttrMinimapColor:
		case DatAttrLensHelp:
		case DatAttrCloth: {
			uint16_t skip = 0;
			return file.getU16(skip);
		}
		case DatAttrLight:
		case DatAttrDisplacement: {
			uint16_t a = 0;
			uint16_t b = 0;
			return file.getU16(a) && file.getU16(b);
		}
		case DatAttrMarket: {
			uint16_t category = 0;
			uint16_t trade_as = 0;
			uint16_t show_as = 0;
			uint16_t name_len = 0;
			uint16_t vocation = 0;
			uint16_t level = 0;
			if (!file.getU16(category) || !file.getU16(trade_as) || !file.getU16(show_as) || !file.getU16(name_len)) {
				return false;
			}
			std::string name;
			if (name_len > 0 && !file.getRAW(name, name_len)) {
				return false;
			}
			return file.getU16(vocation) && file.getU16(level);
		}
		default:
			return true;
	}
}

void WriteThing(::FileWriteHandle& file, const ItemType& type) {
	if (type.ground) {
		file.addU8(DatAttrGround);
		file.addU16(type.ground_speed);
	}
	if (type.container) {
		file.addU8(DatAttrContainer);
	}
	if (type.stackable) {
		file.addU8(DatAttrStackable);
	}
	if (type.not_walkable) {
		file.addU8(DatAttrNotWalkable);
	}
	if (type.not_moveable) {
		file.addU8(DatAttrNotMoveable);
	}
	if (type.block_projectile) {
		file.addU8(DatAttrBlockProjectile);
	}
	if (type.pickupable) {
		file.addU8(DatAttrPickupable);
	}
	if (type.full_ground) {
		file.addU8(DatAttrFullGround);
	}
	if (type.minimap_color != 0) {
		file.addU8(DatAttrMinimapColor);
		file.addU16(type.minimap_color);
	}
	file.addU8(DatAttrLast);
	file.addU8(type.width == 0 ? 1 : type.width);
	file.addU8(type.height == 0 ? 1 : type.height);
	if (type.width > 1 || type.height > 1) {
		file.addU8(32);
	}
	file.addU8(type.layers == 0 ? 1 : type.layers);
	file.addU8(type.pattern_x == 0 ? 1 : type.pattern_x);
	file.addU8(type.pattern_y == 0 ? 1 : type.pattern_y);
	file.addU8(type.pattern_z == 0 ? 1 : type.pattern_z);
	file.addU8(type.phases == 0 ? 1 : type.phases);
	if (!type.sprite_ids.empty()) {
		for (uint16_t sprite : type.sprite_ids) {
			file.addU16(sprite);
		}
	} else {
		file.addU16(type.sprite_id);
	}
}

ItemType MakeSampleItem(uint16_t id) {
	ItemType type;
	type.id = id;
	type.width = 1;
	type.height = 1;
	type.layers = 1;
	type.pattern_x = 1;
	type.pattern_y = 1;
	type.pattern_z = 1;
	type.phases = 1;
	switch (id) {
		case 100:
			type.ground = true;
			type.full_ground = true;
			type.ground_speed = 100;
			type.minimap_color = 24;
			type.sprite_id = 1;
			break;
		case 101:
			type.ground = true;
			type.full_ground = true;
			type.ground_speed = 110;
			type.minimap_color = 121;
			type.sprite_id = 2;
			break;
		case 102:
			type.ground = true;
			type.not_walkable = true;
			type.ground_speed = 0;
			type.minimap_color = 40;
			type.sprite_id = 3;
			break;
		case 103:
			type.not_walkable = true;
			type.not_moveable = true;
			type.block_projectile = true;
			type.minimap_color = 86;
			type.sprite_id = 4;
			break;
		case 104:
			type.pickupable = true;
			type.sprite_id = 5;
			break;
		case 105:
			type.container = true;
			type.pickupable = true;
			type.sprite_id = 6;
			break;
		case 106:
		case 107:
		case 108:
		case 109:
			type.not_walkable = true;
			type.not_moveable = true;
			type.block_projectile = true;
			type.minimap_color = 86;
			type.sprite_id = static_cast<uint16_t>(id - 99);
			break;
		default:
			type.sprite_id = 1;
			break;
	}
	type.sprite_ids = {type.sprite_id};
	ApplyBuiltinName(type);
	return type;
}

} // namespace

void ItemDatabase::clear() {
	items_.clear();
	index_.clear();
	min_id_ = 100;
	max_id_ = 99;
}

const ItemType* ItemDatabase::get(uint16_t id) const {
	const auto it = index_.find(id);
	if (it == index_.end()) {
		return nullptr;
	}
	return &items_[it->second];
}

ItemType* ItemDatabase::get(uint16_t id) {
	const auto it = index_.find(id);
	if (it == index_.end()) {
		return nullptr;
	}
	return &items_[it->second];
}

void ItemDatabase::setName(uint16_t id, std::string name) {
	if (ItemType* type = get(id)) {
		type->name = std::move(name);
	}
}

bool ItemDatabase::parseThing(::FileReadHandle& file, ItemType& type) {
	uint8_t attr = 0;
	while (file.getU8(attr)) {
		if (attr == DatAttrLast) {
			break;
		}
		switch (attr) {
			case DatAttrGround:
				type.ground = true;
				if (!file.getU16(type.ground_speed)) {
					return false;
				}
				break;
			case DatAttrContainer:
				type.container = true;
				break;
			case DatAttrStackable:
				type.stackable = true;
				break;
			case DatAttrNotWalkable:
				type.not_walkable = true;
				break;
			case DatAttrNotMoveable:
				type.not_moveable = true;
				break;
			case DatAttrBlockProjectile:
				type.block_projectile = true;
				break;
			case DatAttrPickupable:
				type.pickupable = true;
				break;
			case DatAttrFullGround:
				type.full_ground = true;
				break;
			case DatAttrMinimapColor:
				if (!file.getU16(type.minimap_color)) {
					return false;
				}
				break;
			default:
				if (!SkipAttrPayload(file, attr)) {
					return false;
				}
				break;
		}
	}
	if (attr != DatAttrLast) {
		return false;
	}

	if (!file.getU8(type.width) || !file.getU8(type.height)) {
		return false;
	}
	if (type.width == 0) {
		type.width = 1;
	}
	if (type.height == 0) {
		type.height = 1;
	}
	if (type.width > 1 || type.height > 1) {
		uint8_t exact = 0;
		if (!file.getU8(exact)) {
			return false;
		}
	}
	if (!file.getU8(type.layers) || !file.getU8(type.pattern_x) || !file.getU8(type.pattern_y)
		|| !file.getU8(type.pattern_z) || !file.getU8(type.phases)) {
		return false;
	}
	if (type.layers == 0) {
		type.layers = 1;
	}
	if (type.pattern_x == 0) {
		type.pattern_x = 1;
	}
	if (type.pattern_y == 0) {
		type.pattern_y = 1;
	}
	if (type.pattern_z == 0) {
		type.pattern_z = 1;
	}
	if (type.phases == 0) {
		type.phases = 1;
	}

	const int sprite_count = static_cast<int>(type.width) * type.height * type.layers
		* type.pattern_x * type.pattern_y * type.pattern_z * type.phases;
	type.sprite_ids.resize(static_cast<std::size_t>(sprite_count));
	for (int i = 0; i < sprite_count; ++i) {
		if (!file.getU16(type.sprite_ids[static_cast<std::size_t>(i)])) {
			return false;
		}
	}
	type.sprite_id = type.sprite_ids.empty() ? 0 : type.sprite_ids.front();
	return true;
}

bool ItemDatabase::load(const std::string& path, ClientAssetsInfo& info) {
	clear();
	info.dat_loaded = false;
	info.dat_path = path;

	::FileReadHandle file(path);
	if (!file.isOk()) {
		info.error = file.getErrorMessage();
		return false;
	}

	uint16_t items16 = 0, outfits16 = 0, effects16 = 0, missiles16 = 0;
	if (!file.getU32(info.dat_signature) || !file.getU16(items16) || !file.getU16(outfits16)
		|| !file.getU16(effects16) || !file.getU16(missiles16)) {
		info.error = "Could not read .dat header";
		return false;
	}

	info.item_count = items16;
	info.outfit_count = outfits16;
	info.effect_count = effects16;
	info.missile_count = missiles16;

	if (items16 < 100) {
		info.error = ".dat item count is below the classic item start id (100)";
		return false;
	}

	min_id_ = 100;
	max_id_ = items16;
	items_.reserve(static_cast<std::size_t>(items16 - 99));

	for (uint32_t id = 100; id <= items16; ++id) {
		ItemType type;
		type.id = static_cast<uint16_t>(id);
		if (!parseThing(file, type)) {
			info.error = "Failed to parse .dat item " + std::to_string(id);
			clear();
			return false;
		}
		ApplyBuiltinName(type);
		index_[id] = items_.size();
		items_.push_back(std::move(type));
	}

	ItemType skip;
	for (uint16_t i = 0; i < outfits16; ++i) {
		if (!parseThing(file, skip)) {
			info.error = "Failed to skip .dat outfit " + std::to_string(i);
			return false;
		}
	}
	for (uint16_t i = 0; i < effects16; ++i) {
		if (!parseThing(file, skip)) {
			info.error = "Failed to skip .dat effect " + std::to_string(i);
			return false;
		}
	}
	for (uint16_t i = 0; i < missiles16; ++i) {
		if (!parseThing(file, skip)) {
			info.error = "Failed to skip .dat missile " + std::to_string(i);
			return false;
		}
	}

	info.dat_loaded = true;
	info.error.clear();
	return true;
}

bool ItemDatabase::writeSample(const std::string& path) {
	const auto parent = std::filesystem::path(path).parent_path();
	if (!parent.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(parent, ec);
	}

	::FileWriteHandle file(path);
	if (!file.isOk()) {
		return false;
	}

	constexpr uint32_t kSampleSignature = 0x00008600;
	constexpr uint16_t kMaxItem = 109;
	file.addU32(kSampleSignature);
	file.addU16(kMaxItem);
	file.addU16(0);
	file.addU16(0);
	file.addU16(0);

	clear();
	min_id_ = 100;
	max_id_ = kMaxItem;
	for (uint16_t id = 100; id <= kMaxItem; ++id) {
		ItemType type = MakeSampleItem(id);
		WriteThing(file, type);
		index_[id] = items_.size();
		items_.push_back(std::move(type));
	}
	file.flush();
	return true;
}

bool LoadDatHeader(const std::string& path, ClientAssetsInfo& info) {
	::FileReadHandle file(path);
	if (!file.isOk()) {
		info.error = file.getErrorMessage();
		info.dat_loaded = false;
		return false;
	}

	if (!file.getU32(info.dat_signature)) {
		info.error = "Could not read .dat signature";
		return false;
	}

	uint16_t items16 = 0, outfits16 = 0, effects16 = 0, missiles16 = 0;
	if (!file.getU16(items16) || !file.getU16(outfits16) || !file.getU16(effects16) || !file.getU16(missiles16)) {
		info.error = "Could not read .dat object counts";
		return false;
	}

	info.item_count = items16;
	info.outfit_count = outfits16;
	info.effect_count = effects16;
	info.missile_count = missiles16;
	info.dat_path = path;
	info.dat_loaded = true;
	info.error.clear();
	return true;
}

bool LoadSprHeader(const std::string& path, ClientAssetsInfo& info) {
	::FileReadHandle file(path);
	if (!file.isOk()) {
		info.error = file.getErrorMessage();
		info.spr_loaded = false;
		return false;
	}

	if (!file.getU32(info.spr_signature)) {
		info.error = "Could not read .spr signature";
		return false;
	}

	auto count_fits = [&](uint32_t count, std::size_t header) {
		return count > 0 && count < 2'000'000 && header + static_cast<std::size_t>(count) * 4 <= file.size();
	};

	uint32_t count32 = 0;
	uint16_t count16 = 0;
	if (file.size() >= 8 && file.getU32(count32) && count_fits(count32, 8)) {
		info.sprite_count = count32;
	} else {
		file.seek(4);
		if (!file.getU16(count16) || !count_fits(count16, 6)) {
			info.error = "Could not read .spr sprite count";
			return false;
		}
		info.sprite_count = count16;
	}

	info.spr_path = path;
	info.spr_loaded = true;
	info.error.clear();
	return true;
}

} // namespace core
} // namespace rme
