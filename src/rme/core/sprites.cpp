#include "sprites.h"

#include "filehandle.h"

#include <algorithm>
#include <filesystem>

namespace rme {
namespace core {
namespace {

bool IsTransparentPixel(const uint8_t* rgba) {
	return rgba[3] == 0 || (rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 255);
}

void PutPixel(std::vector<uint8_t>& rgba, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
	if (x < 0 || y < 0 || x >= kSpriteSize || y >= kSpriteSize) {
		return;
	}
	const int i = (y * kSpriteSize + x) * 4;
	rgba[static_cast<std::size_t>(i) + 0] = r;
	rgba[static_cast<std::size_t>(i) + 1] = g;
	rgba[static_cast<std::size_t>(i) + 2] = b;
	rgba[static_cast<std::size_t>(i) + 3] = a;
}

std::vector<uint8_t> MakePatternedSprite(uint32_t id) {
	std::vector<uint8_t> rgba(static_cast<std::size_t>(kSpriteSize * kSpriteSize * 4), 0);
	for (int y = 0; y < kSpriteSize; ++y) {
		for (int x = 0; x < kSpriteSize; ++x) {
			const bool border = x == 0 || y == 0 || x == kSpriteSize - 1 || y == kSpriteSize - 1;
			uint8_t r = 0, g = 0, b = 0, a = 255;
			switch (id) {
				case 1: { // grass
					const bool speck = ((x * 13 + y * 7) % 11) == 0;
					r = speck ? 46 : 62;
					g = speck ? 92 : 140;
					b = speck ? 28 : 48;
					if (border) {
						r = 36;
						g = 78;
						b = 22;
					}
					break;
				}
				case 2: { // dirt
					const bool speck = ((x * 5 + y * 11) % 9) == 0;
					r = speck ? 96 : 132;
					g = speck ? 70 : 96;
					b = speck ? 36 : 48;
					if (border) {
						r = 78;
						g = 52;
						b = 28;
					}
					break;
				}
				case 3: { // water
					const bool wave = ((x + y / 2) % 8) < 3;
					r = wave ? 36 : 24;
					g = wave ? 92 : 70;
					b = wave ? 176 : 148;
					if (border) {
						r = 16;
						g = 48;
						b = 110;
					}
					break;
				}
				case 4: { // wall / brick
					const bool mortar = (y % 8 == 0) || ((x + (y / 8) * 8) % 16 == 0);
					r = mortar ? 70 : 118;
					g = mortar ? 68 : 108;
					b = mortar ? 66 : 98;
					if (border) {
						r = 48;
						g = 46;
						b = 44;
					}
					break;
				}
				case 5: { // flower
					const int dx = x - 16;
					const int dy = y - 14;
					const int d2 = dx * dx + dy * dy;
					if (d2 < 28) {
						r = 220;
						g = 72;
						b = 118;
					} else if (d2 < 70) {
						r = 240;
						g = 196;
						b = 72;
					} else if (y > 18 && x > 13 && x < 19) {
						r = 48;
						g = 120;
						b = 42;
					} else {
						a = 0;
						r = 255;
						g = 0;
						b = 255;
					}
					break;
				}
				case 6: { // box
					if (x < 4 || x > 27 || y < 6 || y > 29) {
						a = 0;
						r = 255;
						g = 0;
						b = 255;
					} else {
						const bool lid = y < 12;
						const bool edge = x == 4 || x == 27 || y == 6 || y == 11 || y == 29;
						r = edge ? 92 : (lid ? 186 : 156);
						g = edge ? 64 : (lid ? 132 : 108);
						b = edge ? 32 : (lid ? 64 : 52);
					}
					break;
				}
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
				case 16:
				case 17:
				case 18: { // water shores: N E S W / NE SE SW NW
					const bool n = id == 11 || id == 15 || id == 18;
					const bool e = id == 12 || id == 15 || id == 16;
					const bool s = id == 13 || id == 16 || id == 17;
					const bool w = id == 14 || id == 17 || id == 18;
					const bool band = (n && y < 8) || (e && x >= 24) || (s && y >= 24) || (w && x < 8);
					if (band) {
						const bool foam = ((x + y) % 4) == 0;
						r = foam ? 236 : 198;
						g = foam ? 214 : 168;
						b = foam ? 126 : 88;
					} else {
						a = 0;
						r = 255;
						g = 0;
						b = 255;
					}
					break;
				}
				case 7:
				case 8:
				case 9:
				case 10: { // timber pole / h / v / junction
					const bool hbeam = (id == 8 || id == 10) && y >= 12 && y <= 19;
					const bool vbeam = (id == 9 || id == 10) && x >= 12 && x <= 19;
					const bool pole = id == 7 && x >= 11 && x <= 20 && y >= 11 && y <= 20;
					if (hbeam || vbeam || pole) {
						const bool grain = ((x + y) % 5) == 0;
						const bool edge = (hbeam && (y == 12 || y == 19)) || (vbeam && (x == 12 || x == 19))
							|| (pole && (x == 11 || x == 20 || y == 11 || y == 20));
						r = edge ? 92 : (grain ? 168 : 150);
						g = edge ? 62 : (grain ? 112 : 96);
						b = edge ? 28 : (grain ? 54 : 42);
					} else {
						a = 0;
						r = 255;
						g = 0;
						b = 255;
					}
					break;
				}
				default:
					r = static_cast<uint8_t>(40 + (id * 37) % 140);
					g = static_cast<uint8_t>(70 + (id * 17) % 120);
					b = static_cast<uint8_t>(50 + (id * 53) % 130);
					if (border) {
						r = static_cast<uint8_t>(r / 2);
						g = static_cast<uint8_t>(g / 2);
						b = static_cast<uint8_t>(b / 2);
					}
					break;
			}
			PutPixel(rgba, x, y, r, g, b, a);
		}
	}
	return rgba;
}

bool CountFits(uint32_t count, std::size_t header, std::size_t file_size) {
	return count > 0 && count < 2'000'000 && header + static_cast<std::size_t>(count) * 4 <= file_size;
}

} // namespace

void SpriteSheet::clear() {
	sprites_.clear();
	count_ = 0;
}

const Sprite* SpriteSheet::get(uint32_t id) const {
	if (id == 0 || id >= sprites_.size()) {
		return nullptr;
	}
	const Sprite& sprite = sprites_[id];
	return sprite.empty() ? nullptr : &sprite;
}

std::vector<uint8_t> SpriteSheet::MakeSolidSprite(uint8_t r, uint8_t g, uint8_t b, uint8_t border) {
	std::vector<uint8_t> rgba(static_cast<std::size_t>(kSpriteSize * kSpriteSize * 4), 255);
	for (int y = 0; y < kSpriteSize; ++y) {
		for (int x = 0; x < kSpriteSize; ++x) {
			const bool edge = x == 0 || y == 0 || x == kSpriteSize - 1 || y == kSpriteSize - 1;
			const int i = (y * kSpriteSize + x) * 4;
			rgba[static_cast<std::size_t>(i) + 0] = edge ? border : r;
			rgba[static_cast<std::size_t>(i) + 1] = edge ? border : g;
			rgba[static_cast<std::size_t>(i) + 2] = edge ? border : b;
			rgba[static_cast<std::size_t>(i) + 3] = 255;
		}
	}
	return rgba;
}

bool SpriteSheet::DecodeSprite(const uint8_t* data, std::size_t size, Sprite& out) {
	out.width = kSpriteSize;
	out.height = kSpriteSize;
	out.rgba.assign(static_cast<std::size_t>(kSpriteSize * kSpriteSize * 4), 0);

	std::size_t read = 0;
	int pixel = 0;
	constexpr int kTotal = kSpriteSize * kSpriteSize;
	while (read + 4 <= size && pixel < kTotal) {
		const uint16_t transparent = static_cast<uint16_t>(data[read] | (data[read + 1] << 8));
		const uint16_t colored = static_cast<uint16_t>(data[read + 2] | (data[read + 3] << 8));
		read += 4;
		pixel += transparent;
		if (read + static_cast<std::size_t>(colored) * 3 > size) {
			return false;
		}
		for (uint16_t i = 0; i < colored && pixel < kTotal; ++i) {
			const int idx = pixel * 4;
			out.rgba[static_cast<std::size_t>(idx) + 0] = data[read++];
			out.rgba[static_cast<std::size_t>(idx) + 1] = data[read++];
			out.rgba[static_cast<std::size_t>(idx) + 2] = data[read++];
			out.rgba[static_cast<std::size_t>(idx) + 3] = 255;
			++pixel;
		}
	}
	return true;
}

std::vector<uint8_t> SpriteSheet::EncodeSprite(const std::vector<uint8_t>& rgba) {
	std::vector<uint8_t> out;
	const int total = static_cast<int>(rgba.size() / 4);
	int i = 0;
	while (i < total) {
		uint16_t transparent = 0;
		while (i < total && IsTransparentPixel(rgba.data() + static_cast<std::size_t>(i) * 4)) {
			++transparent;
			++i;
		}
		const int colored_start = i;
		uint16_t colored = 0;
		while (i < total && !IsTransparentPixel(rgba.data() + static_cast<std::size_t>(i) * 4)) {
			++colored;
			++i;
		}
		if (colored == 0) {
			break;
		}
		out.push_back(static_cast<uint8_t>(transparent & 0xFF));
		out.push_back(static_cast<uint8_t>(transparent >> 8));
		out.push_back(static_cast<uint8_t>(colored & 0xFF));
		out.push_back(static_cast<uint8_t>(colored >> 8));
		for (uint16_t p = 0; p < colored; ++p) {
			const int o = (colored_start + p) * 4;
			out.push_back(rgba[static_cast<std::size_t>(o) + 0]);
			out.push_back(rgba[static_cast<std::size_t>(o) + 1]);
			out.push_back(rgba[static_cast<std::size_t>(o) + 2]);
		}
	}
	return out;
}

bool SpriteSheet::load(const std::string& path, uint32_t& signature, uint32_t& count, std::string& error) {
	clear();
	FileReadHandle file(path);
	if (!file.isOk()) {
		error = file.getErrorMessage();
		return false;
	}

	if (!file.getU32(signature)) {
		error = "Could not read .spr signature";
		return false;
	}

	uint32_t count32 = 0;
	uint16_t count16 = 0;
	bool used_u32 = false;
	if (file.size() >= 8 && file.getU32(count32) && CountFits(count32, 8, file.size())) {
		count = count32;
		used_u32 = true;
	} else {
		file.seek(4);
		if (!file.getU16(count16) || !CountFits(count16, 6, file.size())) {
			error = "Could not read .spr sprite count";
			return false;
		}
		count = count16;
	}

	std::vector<uint32_t> offsets(count, 0);
	for (uint32_t i = 0; i < count; ++i) {
		if (!file.getU32(offsets[i])) {
			error = "Could not read .spr offset table";
			clear();
			return false;
		}
	}
	(void)used_u32;

	sprites_.assign(count + 1, Sprite{});
	count_ = count;
	for (uint32_t id = 1; id <= count; ++id) {
		const uint32_t offset = offsets[id - 1];
		if (offset == 0 || offset + 5 > file.size()) {
			continue;
		}
		if (!file.seek(offset)) {
			continue;
		}
		uint8_t key_r = 0, key_g = 0, key_b = 0;
		uint16_t payload = 0;
		if (!file.getU8(key_r) || !file.getU8(key_g) || !file.getU8(key_b) || !file.getU16(payload)) {
			continue;
		}
		(void)key_r;
		(void)key_g;
		(void)key_b;
		if (payload == 0) {
			continue;
		}
		std::vector<uint8_t> data(payload);
		if (!file.getRAW(data.data(), payload)) {
			continue;
		}
		Sprite sprite;
		sprite.id = id;
		if (DecodeSprite(data.data(), data.size(), sprite)) {
			sprites_[id] = std::move(sprite);
		}
	}

	error.clear();
	return true;
}

bool SpriteSheet::writeSample(const std::string& path) {
	const auto parent = std::filesystem::path(path).parent_path();
	if (!parent.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(parent, ec);
	}

	constexpr uint32_t kSampleSignature = 0x00008600;
	constexpr uint16_t kCount = 18;

	clear();
	count_ = kCount;
	sprites_.assign(kCount + 1, Sprite{});
	for (uint32_t id = 1; id <= kCount; ++id) {
		Sprite sprite;
		sprite.id = id;
		sprite.width = kSpriteSize;
		sprite.height = kSpriteSize;
		sprite.rgba = MakePatternedSprite(id);
		sprites_[id] = std::move(sprite);
	}

	std::vector<std::vector<uint8_t>> encoded(kCount);
	for (uint16_t id = 1; id <= kCount; ++id) {
		encoded[id - 1] = EncodeSprite(sprites_[id].rgba);
	}

	FileWriteHandle file(path);
	if (!file.isOk()) {
		clear();
		return false;
	}

	file.addU32(kSampleSignature);
	file.addU16(kCount);
	uint32_t cursor = 6 + static_cast<uint32_t>(kCount) * 4;
	for (uint16_t id = 1; id <= kCount; ++id) {
		file.addU32(cursor);
		cursor += 5 + static_cast<uint32_t>(encoded[id - 1].size());
	}
	for (uint16_t id = 1; id <= kCount; ++id) {
		file.addU8(255);
		file.addU8(0);
		file.addU8(255);
		file.addU16(static_cast<uint16_t>(encoded[id - 1].size()));
		if (!encoded[id - 1].empty()) {
			file.addRAW(encoded[id - 1].data(), encoded[id - 1].size());
		}
	}
	file.flush();
	return true;
}

} // namespace core
} // namespace rme
