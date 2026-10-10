#pragma once

#include "item_type.h"

class FileReadHandle;

#include <cstdint>
#include <string>
#include <string_view>
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

inline char PaletteAsciiLower(char c) {
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

inline std::string_view TrimPaletteQuery(std::string_view query) {
	while (!query.empty() && (query.front() == ' ' || query.front() == '\t')) {
		query.remove_prefix(1);
	}
	while (!query.empty() && (query.back() == ' ' || query.back() == '\t')) {
		query.remove_suffix(1);
	}
	return query;
}

inline bool PaletteContainsInsensitive(std::string_view haystack, std::string_view needle) {
	if (needle.empty()) {
		return true;
	}
	if (needle.size() > haystack.size()) {
		return false;
	}
	for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
		bool match = true;
		for (std::size_t j = 0; j < needle.size(); ++j) {
			if (PaletteAsciiLower(haystack[i + j]) != PaletteAsciiLower(needle[j])) {
				match = false;
				break;
			}
		}
		if (match) {
			return true;
		}
	}
	return false;
}

inline bool ItemMatchesPaletteQuery(const ItemType& type, std::string_view query) {
	query = TrimPaletteQuery(query);
	if (query.empty()) {
		return true;
	}
	if (PaletteContainsInsensitive(type.name, query)) {
		return true;
	}
	return PaletteContainsInsensitive(std::to_string(type.id), query);
}

} // namespace core
} // namespace rme
