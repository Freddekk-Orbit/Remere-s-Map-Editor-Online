#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rme {
namespace core {

struct ZipEntry {
	std::string name;
	std::vector<uint8_t> data;
};

bool WriteStoreZip(const std::string& path, const std::vector<ZipEntry>& entries);
bool ReadStoreZip(const std::string& path, std::vector<ZipEntry>& entries);
bool LoadFileBytes(const std::string& path, std::vector<uint8_t>& out);
std::string MapOtbmFileName(const std::string& name);
std::string MapZipFileName(const std::string& name);

} // namespace core
} // namespace rme
