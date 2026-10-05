#pragma once

#include <cstdint>
#include <string>

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

bool LoadDatHeader(const std::string& path, ClientAssetsInfo& info);
bool LoadSprHeader(const std::string& path, ClientAssetsInfo& info);

} // namespace core
} // namespace rme
