#include "zip.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace rme {
namespace core {
namespace {

uint32_t Crc32(const uint8_t* data, std::size_t size) {
	uint32_t crc = 0xFFFFFFFFu;
	for (std::size_t i = 0; i < size; ++i) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; ++bit) {
			const uint32_t mask = 0u - (crc & 1u);
			crc = (crc >> 1) ^ (0xEDB88320u & mask);
		}
	}
	return ~crc;
}

void PutU16(std::vector<uint8_t>& out, uint16_t value) {
	out.push_back(static_cast<uint8_t>(value));
	out.push_back(static_cast<uint8_t>(value >> 8));
}

void PutU32(std::vector<uint8_t>& out, uint32_t value) {
	out.push_back(static_cast<uint8_t>(value));
	out.push_back(static_cast<uint8_t>(value >> 8));
	out.push_back(static_cast<uint8_t>(value >> 16));
	out.push_back(static_cast<uint8_t>(value >> 24));
}

bool GetU16(const std::vector<uint8_t>& in, std::size_t& offset, uint16_t& value) {
	if (offset + 2 > in.size()) {
		return false;
	}
	value = static_cast<uint16_t>(in[offset] | (in[offset + 1] << 8));
	offset += 2;
	return true;
}

bool GetU32(const std::vector<uint8_t>& in, std::size_t& offset, uint32_t& value) {
	if (offset + 4 > in.size()) {
		return false;
	}
	value = static_cast<uint32_t>(in[offset] | (in[offset + 1] << 8) | (in[offset + 2] << 16) | (in[offset + 3] << 24));
	offset += 4;
	return true;
}

} // namespace

bool LoadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
	FILE* file = std::fopen(path.c_str(), "rb");
	if (!file) {
		return false;
	}
	std::fseek(file, 0, SEEK_END);
	const long size = std::ftell(file);
	if (size < 0) {
		std::fclose(file);
		return false;
	}
	std::fseek(file, 0, SEEK_SET);
	out.resize(static_cast<std::size_t>(size));
	const auto read = std::fread(out.data(), 1, out.size(), file);
	std::fclose(file);
	return read == out.size();
}

namespace {

bool WriteAllBytes(const std::string& path, const std::vector<uint8_t>& bytes) {
	const auto parent = std::filesystem::path(path).parent_path();
	if (!parent.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(parent, ec);
	}
	FILE* file = std::fopen(path.c_str(), "wb");
	if (!file) {
		return false;
	}
	const auto written = std::fwrite(bytes.data(), 1, bytes.size(), file);
	std::fclose(file);
	return written == bytes.size();
}

std::string SanitizeStem(std::string name) {
	name = std::filesystem::path(name).filename().string();
	auto lower = name;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	if (lower.ends_with(".otbm")) {
		name.erase(name.size() - 5);
	} else if (lower.ends_with(".zip")) {
		name.erase(name.size() - 4);
	}
	for (char& ch : name) {
		if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>'
			|| ch == '|' || ch == '\0') {
			ch = '_';
		}
	}
	while (!name.empty() && (name.back() == ' ' || name.back() == '.')) {
		name.pop_back();
	}
	if (name.empty()) {
		return "Untitled";
	}
	return name;
}

} // namespace

std::string MapOtbmFileName(const std::string& name) {
	return SanitizeStem(name) + ".otbm";
}

std::string MapZipFileName(const std::string& name) {
	return SanitizeStem(name) + ".zip";
}

bool WriteStoreZip(const std::string& path, const std::vector<ZipEntry>& entries) {
	std::vector<uint8_t> out;
	struct Meta {
		std::string name;
		uint32_t crc = 0;
		uint32_t size = 0;
		uint32_t offset = 0;
	};
	std::vector<Meta> metas;
	metas.reserve(entries.size());

	for (const ZipEntry& entry : entries) {
		if (entry.name.empty()) {
			continue;
		}
		Meta meta;
		meta.name = std::filesystem::path(entry.name).filename().string();
		meta.crc = Crc32(entry.data.data(), entry.data.size());
		meta.size = static_cast<uint32_t>(entry.data.size());
		meta.offset = static_cast<uint32_t>(out.size());

		out.insert(out.end(), {'P', 'K', 0x03, 0x04});
		PutU16(out, 20);
		PutU16(out, 0);
		PutU16(out, 0);
		PutU16(out, 0);
		PutU16(out, 0);
		PutU32(out, meta.crc);
		PutU32(out, meta.size);
		PutU32(out, meta.size);
		PutU16(out, static_cast<uint16_t>(meta.name.size()));
		PutU16(out, 0);
		out.insert(out.end(), meta.name.begin(), meta.name.end());
		out.insert(out.end(), entry.data.begin(), entry.data.end());
		metas.push_back(std::move(meta));
	}

	const uint32_t central_offset = static_cast<uint32_t>(out.size());
	for (const Meta& meta : metas) {
		out.insert(out.end(), {'P', 'K', 0x01, 0x02});
		PutU16(out, 20);
		PutU16(out, 20);
		PutU16(out, 0);
		PutU16(out, 0);
		PutU16(out, 0);
		PutU16(out, 0);
		PutU32(out, meta.crc);
		PutU32(out, meta.size);
		PutU32(out, meta.size);
		PutU16(out, static_cast<uint16_t>(meta.name.size()));
		PutU16(out, 0);
		PutU16(out, 0);
		PutU16(out, 0);
		PutU16(out, 0);
		PutU32(out, 0);
		PutU32(out, meta.offset);
		out.insert(out.end(), meta.name.begin(), meta.name.end());
	}
	const uint32_t central_size = static_cast<uint32_t>(out.size() - central_offset);
	out.insert(out.end(), {'P', 'K', 0x05, 0x06});
	PutU16(out, 0);
	PutU16(out, 0);
	PutU16(out, static_cast<uint16_t>(metas.size()));
	PutU16(out, static_cast<uint16_t>(metas.size()));
	PutU32(out, central_size);
	PutU32(out, central_offset);
	PutU16(out, 0);
	return WriteAllBytes(path, out);
}

bool ReadStoreZip(const std::string& path, std::vector<ZipEntry>& entries) {
	std::vector<uint8_t> bytes;
	if (!LoadFileBytes(path, bytes) || bytes.size() < 22) {
		return false;
	}

	std::size_t eocd = bytes.size();
	bool found = false;
	for (std::size_t i = bytes.size(); i >= 22; --i) {
		const std::size_t pos = i - 22;
		if (bytes[pos] == 'P' && bytes[pos + 1] == 'K' && bytes[pos + 2] == 0x05 && bytes[pos + 3] == 0x06) {
			eocd = pos;
			found = true;
			break;
		}
		if (bytes.size() - pos > 0x10016) {
			break;
		}
	}
	if (!found) {
		return false;
	}

	std::size_t offset = eocd + 4;
	uint16_t disk = 0, start_disk = 0, entries_here = 0, entries_total = 0;
	uint32_t central_size = 0, central_offset = 0;
	uint16_t comment = 0;
	if (!GetU16(bytes, offset, disk) || !GetU16(bytes, offset, start_disk) || !GetU16(bytes, offset, entries_here)
		|| !GetU16(bytes, offset, entries_total) || !GetU32(bytes, offset, central_size)
		|| !GetU32(bytes, offset, central_offset) || !GetU16(bytes, offset, comment)) {
		return false;
	}
	(void)disk;
	(void)start_disk;
	(void)entries_total;
	(void)central_size;
	(void)comment;

	entries.clear();
	offset = central_offset;
	for (uint16_t i = 0; i < entries_here; ++i) {
		if (offset + 46 > bytes.size() || bytes[offset] != 'P' || bytes[offset + 1] != 'K' || bytes[offset + 2] != 0x01
			|| bytes[offset + 3] != 0x02) {
			return false;
		}
		offset += 4;
		uint16_t made = 0, need = 0, flags = 0, method = 0, time = 0, date = 0, name_len = 0, extra_len = 0, comment_len = 0;
		uint16_t disk_start = 0, int_attr = 0;
		uint32_t crc = 0, comp = 0, uncomp = 0, ext_attr = 0, local_offset = 0;
		if (!GetU16(bytes, offset, made) || !GetU16(bytes, offset, need) || !GetU16(bytes, offset, flags)
			|| !GetU16(bytes, offset, method) || !GetU16(bytes, offset, time) || !GetU16(bytes, offset, date)
			|| !GetU32(bytes, offset, crc) || !GetU32(bytes, offset, comp) || !GetU32(bytes, offset, uncomp)
			|| !GetU16(bytes, offset, name_len) || !GetU16(bytes, offset, extra_len) || !GetU16(bytes, offset, comment_len)
			|| !GetU16(bytes, offset, disk_start) || !GetU16(bytes, offset, int_attr) || !GetU32(bytes, offset, ext_attr)
			|| !GetU32(bytes, offset, local_offset)) {
			return false;
		}
		(void)made;
		(void)need;
		(void)flags;
		(void)time;
		(void)date;
		(void)crc;
		(void)disk_start;
		(void)int_attr;
		(void)ext_attr;
		if (method != 0 || offset + name_len > bytes.size()) {
			return false;
		}
		ZipEntry entry;
		entry.name.assign(reinterpret_cast<const char*>(bytes.data() + offset), name_len);
		offset += name_len + extra_len + comment_len;

		std::size_t local = local_offset;
		if (local + 30 > bytes.size() || bytes[local] != 'P' || bytes[local + 1] != 'K' || bytes[local + 2] != 0x03
			|| bytes[local + 3] != 0x04) {
			return false;
		}
		local += 4 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4;
		uint16_t local_name = 0, local_extra = 0;
		if (!GetU16(bytes, local, local_name) || !GetU16(bytes, local, local_extra)) {
			return false;
		}
		local += local_name + local_extra;
		if (local + uncomp > bytes.size()) {
			return false;
		}
		entry.data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(local),
			bytes.begin() + static_cast<std::ptrdiff_t>(local + uncomp));
		entries.push_back(std::move(entry));
	}
	return true;
}

} // namespace core
} // namespace rme
