#pragma once

#include "item_type.h"

class FileReadHandle;

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rme {
namespace core {

struct ClientAssetsInfo {
	bool dat_loaded = false;
	bool spr_loaded = false;
	uint32_t dat_signature = 0;
	uint32_t spr_signature = 0;
	uint32_t item_count = 0;
	uint32_t outfit_count = 0;
	uint32_t effect_count = 0;
	uint32_t missile_count = 0;
	uint32_t sprite_count = 0;
	std::string dat_path;
	std::string spr_path;
	std::string error;
};

class ItemDatabase {
public:
	void clear();
	bool load(const std::string& path, ClientAssetsInfo& info);
	bool writeSample(const std::string& path);

	const ItemType* get(uint16_t id) const;
	ItemType* get(uint16_t id);
	void setName(uint16_t id, std::string name);

	const std::vector<ItemType>& items() const { return items_; }
	uint16_t minId() const { return min_id_; }
	uint16_t maxId() const { return max_id_; }
	std::size_t size() const { return items_.size(); }

private:
	bool parseThing(::FileReadHandle& file, ItemType& type);

	std::vector<ItemType> items_;
	std::unordered_map<uint16_t, std::size_t> index_;
	uint16_t min_id_ = 100;
	uint16_t max_id_ = 99;
};

bool LoadDatHeader(const std::string& path, ClientAssetsInfo& info);
bool LoadSprHeader(const std::string& path, ClientAssetsInfo& info);

} // namespace core
} // namespace rme
