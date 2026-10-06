#pragma once

// Classic TFS/RME companion XML for houses and monster spawns.
// Parser is intentionally tiny and only understands files this port writes.

#include "map.h"

#include <string>

namespace rme {
namespace core {

std::string CompanionPath(const std::string& otbm_path, const std::string& filename, const std::string& fallback = {});

bool LoadHouseXml(Map& map, const std::string& path);
bool SaveHouseXml(const Map& map, const std::string& path);
bool LoadSpawnXml(Map& map, const std::string& path);
bool SaveSpawnXml(const Map& map, const std::string& path);

} // namespace core
} // namespace rme
