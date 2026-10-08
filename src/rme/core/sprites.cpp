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

void ClearMagenta(std::vector<uint8_t>& rgba) {
	for (int y = 0; y < kSpriteSize; ++y) {
		for (int x = 0; x < kSpriteSize; ++x) {
			PutPixel(rgba, x, y, 255, 0, 255, 0);
		}
	}
}

uint8_t Hash8(int x, int y, int salt) {
	return static_cast<uint8_t>((x * 37 + y * 17 + salt * 13) & 255);
}

void WoodPixel(uint8_t& r, uint8_t& g, uint8_t& b, int x, int y, bool dark_edge) {
	const bool grain = ((x + y / 2) % 6) == 0;
	r = dark_edge ? 86 : (grain ? 168 : 150);
	g = dark_edge ? 54 : (grain ? 108 : 92);
	b = dark_edge ? 24 : (grain ? 48 : 38);
}

void DrawGround(std::vector<uint8_t>& rgba, uint32_t id) {
	for (int y = 0; y < kSpriteSize; ++y) {
		for (int x = 0; x < kSpriteSize; ++x) {
			uint8_t r = 0, g = 0, b = 0;
			if (id == 1) {
				const uint8_t h = Hash8(x, y, 3);
				r = static_cast<uint8_t>(48 + (h % 22));
				g = static_cast<uint8_t>(96 + (h % 40));
				b = static_cast<uint8_t>(28 + (h % 16));
				if (h > 230) {
					r = 92;
					g = 148;
					b = 36;
				}
			} else if (id == 2) {
				const uint8_t h = Hash8(x, y, 9);
				r = static_cast<uint8_t>(118 + (h % 28));
				g = static_cast<uint8_t>(82 + (h % 20));
				b = static_cast<uint8_t>(42 + (h % 14));
				if (h > 240) {
					r = 86;
					g = 62;
					b = 32;
				}
			} else {
				const bool wave = ((x + (y / 3) * 2) % 10) < 4;
				r = wave ? 32 : 20;
				g = wave ? 88 : 64;
				b = wave ? 168 : 140;
				if (((x + y) % 17) == 0) {
					r = 48;
					g = 120;
					b = 196;
				}
			}
			PutPixel(rgba, x, y, r, g, b);
		}
	}
}

void DrawBrick(std::vector<uint8_t>& rgba) {
	for (int y = 0; y < kSpriteSize; ++y) {
		for (int x = 0; x < kSpriteSize; ++x) {
			const int row = y / 8;
			const int shift = (row % 2) * 8;
			const bool mortar = (y % 8 == 0) || ((x + shift) % 16 == 0);
			const uint8_t h = Hash8(x, y, 4) % 18;
			PutPixel(rgba, x, y, mortar ? 64 : static_cast<uint8_t>(128 + h), mortar ? 60 : static_cast<uint8_t>(108 + h / 2),
				mortar ? 56 : static_cast<uint8_t>(92 + h / 3));
		}
	}
}

void DrawFlower(std::vector<uint8_t>& rgba) {
	ClearMagenta(rgba);
	for (int y = 20; y < 31; ++y) {
		PutPixel(rgba, 16, y, 42, 112, 38);
		PutPixel(rgba, 15, y, 36, 96, 32);
	}
	PutPixel(rgba, 12, 24, 48, 132, 44);
	PutPixel(rgba, 11, 24, 48, 132, 44);
	PutPixel(rgba, 12, 25, 40, 110, 36);
	PutPixel(rgba, 20, 23, 48, 132, 44);
	PutPixel(rgba, 21, 23, 40, 110, 36);
	const int petals[8][2] = {{16, 8}, {20, 10}, {22, 14}, {20, 18}, {16, 20}, {12, 18}, {10, 14}, {12, 10}};
	for (const auto& p : petals) {
		for (int dy = -3; dy <= 3; ++dy) {
			for (int dx = -3; dx <= 3; ++dx) {
				if (dx * dx + dy * dy <= 8) {
					PutPixel(rgba, p[0] + dx, p[1] + dy, 220, 58, 78);
				}
			}
		}
	}
	for (int dy = -3; dy <= 3; ++dy) {
		for (int dx = -3; dx <= 3; ++dx) {
			if (dx * dx + dy * dy <= 6) {
				PutPixel(rgba, 16 + dx, 14 + dy, 236, 196, 48);
			}
		}
	}
}

void DrawBox(std::vector<uint8_t>& rgba) {
	ClearMagenta(rgba);
	for (int y = 7; y <= 29; ++y) {
		for (int x = 5; x <= 26; ++x) {
			const bool lid = y <= 12;
			const bool edge = x == 5 || x == 26 || y == 7 || y == 12 || y == 29;
			uint8_t r, g, b;
			WoodPixel(r, g, b, x, y, edge);
			if (lid && !edge) {
				r = static_cast<uint8_t>(r + 20);
				g = static_cast<uint8_t>(g + 14);
			}
			PutPixel(rgba, x, y, r, g, b);
		}
	}
}

void DrawTimber(std::vector<uint8_t>& rgba, uint32_t id) {
	ClearMagenta(rgba);
	const bool hbeam = id == 8 || id == 10;
	const bool vbeam = id == 9 || id == 10;
	const bool pole = id == 7;
	for (int y = 0; y < kSpriteSize; ++y) {
		for (int x = 0; x < kSpriteSize; ++x) {
			const bool h = hbeam && y >= 11 && y <= 20;
			const bool v = vbeam && x >= 11 && x <= 20;
			const bool p = pole && x >= 10 && x <= 21 && y >= 10 && y <= 21;
			if (!(h || v || p)) {
				continue;
			}
			const bool edge = (h && (y == 11 || y == 20)) || (v && (x == 11 || x == 20))
				|| (p && (x == 10 || x == 21 || y == 10 || y == 21));
			uint8_t r, g, b;
			WoodPixel(r, g, b, x, y, edge);
			PutPixel(rgba, x, y, r, g, b);
		}
	}
}

void DrawShore(std::vector<uint8_t>& rgba, uint32_t id) {
	ClearMagenta(rgba);
	const bool n = id == 11 || id == 15 || id == 18;
	const bool e = id == 12 || id == 15 || id == 16;
	const bool s = id == 13 || id == 16 || id == 17;
	const bool w = id == 14 || id == 17 || id == 18;
	for (int y = 0; y < kSpriteSize; ++y) {
		for (int x = 0; x < kSpriteSize; ++x) {
			const bool band = (n && y < 9) || (e && x >= 23) || (s && y >= 23) || (w && x < 9);
			if (!band) {
				continue;
			}
			const uint8_t h = Hash8(x, y, 2);
			const bool foam = (h % 5) == 0;
			PutPixel(rgba, x, y, foam ? 232 : static_cast<uint8_t>(186 + h % 24),
				foam ? 208 : static_cast<uint8_t>(150 + h % 20), foam ? 118 : static_cast<uint8_t>(78 + h % 16));
		}
	}
}

void DrawDoor(std::vector<uint8_t>& rgba, bool horizontal) {
	ClearMagenta(rgba);
	const int x0 = horizontal ? 2 : 9;
	const int x1 = horizontal ? 29 : 22;
	const int y0 = horizontal ? 8 : 2;
	const int y1 = horizontal ? 23 : 29;
	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			const bool edge = x == x0 || x == x1 || y == y0 || y == y1;
			const bool panel = horizontal ? ((x - x0) % 7 == 0) : ((y - y0) % 7 == 0);
			uint8_t r, g, b;
			WoodPixel(r, g, b, x, y, edge || panel);
			PutPixel(rgba, x, y, r, g, b);
		}
	}
	if (horizontal) {
		PutPixel(rgba, 24, 15, 196, 164, 48);
		PutPixel(rgba, 25, 15, 196, 164, 48);
		PutPixel(rgba, 24, 16, 160, 128, 32);
	} else {
		PutPixel(rgba, 18, 16, 196, 164, 48);
		PutPixel(rgba, 19, 16, 196, 164, 48);
		PutPixel(rgba, 18, 17, 160, 128, 32);
	}
}

void DrawTable(std::vector<uint8_t>& rgba, uint32_t id) {
	ClearMagenta(rgba);
	const bool h = id == 22 || id == 24;
	const bool v = id == 23 || id == 24;
	const bool single = id == 21;
	const int top0 = 10;
	const int top1 = 21;
	for (int y = top0; y <= top1; ++y) {
		for (int x = (h || single ? 0 : 8); x <= (h || single ? 31 : 23); ++x) {
			if (!(h || single) && (x < 8 || x > 23)) {
				continue;
			}
			if (!(v || single) && (y < top0 || y > top1) && !(h)) {
				continue;
			}
			const bool edge = y == top0 || y == top1 || ((single || !h) && (x == 8 || x == 23))
				|| (single && (x == 0 || x == 31));
			uint8_t r, g, b;
			WoodPixel(r, g, b, x, y, edge);
			r = static_cast<uint8_t>(std::min(255, r + 18));
			g = static_cast<uint8_t>(std::min(255, g + 10));
			PutPixel(rgba, x, y, r, g, b);
		}
	}
	if (v) {
		for (int y = 0; y < kSpriteSize; ++y) {
			for (int x = 10; x <= 21; ++x) {
				if (y >= top0 && y <= top1) {
					continue;
				}
				uint8_t r, g, b;
				WoodPixel(r, g, b, x, y, x == 10 || x == 21);
				r = static_cast<uint8_t>(std::min(255, r + 18));
				g = static_cast<uint8_t>(std::min(255, g + 10));
				PutPixel(rgba, x, y, r, g, b);
			}
		}
	}
	if (single || (!h && !v)) {
		for (int y = 22; y <= 29; ++y) {
			PutPixel(rgba, 9, y, 96, 62, 28);
			PutPixel(rgba, 22, y, 96, 62, 28);
		}
	}
}

void DrawCarpet(std::vector<uint8_t>& rgba, uint32_t id) {
	const bool n = id == 26 || id == 30 || id == 33;
	const bool e = id == 27 || id == 30 || id == 31;
	const bool s = id == 28 || id == 31 || id == 32;
	const bool w = id == 29 || id == 32 || id == 33;
	const bool inner = id == 25;
	for (int y = 0; y < kSpriteSize; ++y) {
		for (int x = 0; x < kSpriteSize; ++x) {
			const bool trim = (!inner) && ((n && y < 4) || (e && x >= 28) || (s && y >= 28) || (w && x < 4));
			const uint8_t h = Hash8(x, y, 7);
			uint8_t r = trim ? 92 : static_cast<uint8_t>(148 + (h % 18));
			uint8_t g = trim ? 24 : static_cast<uint8_t>(36 + (h % 10));
			uint8_t b = trim ? 24 : static_cast<uint8_t>(36 + (h % 8));
			if (!trim && ((x + y) % 8 == 0)) {
				r = static_cast<uint8_t>(r + 20);
			}
			PutPixel(rgba, x, y, r, g, b);
		}
	}
}

std::vector<uint8_t> MakePatternedSprite(uint32_t id) {
	std::vector<uint8_t> rgba(static_cast<std::size_t>(kSpriteSize * kSpriteSize * 4), 0);
	switch (id) {
		case 1:
		case 2:
		case 3:
			DrawGround(rgba, id);
			break;
		case 4:
			DrawBrick(rgba);
			break;
		case 5:
			DrawFlower(rgba);
			break;
		case 6:
			DrawBox(rgba);
			break;
		case 7:
		case 8:
		case 9:
		case 10:
			DrawTimber(rgba, id);
			break;
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:
			DrawShore(rgba, id);
			break;
		case 19:
			DrawDoor(rgba, true);
			break;
		case 20:
			DrawDoor(rgba, false);
			break;
		case 21:
		case 22:
		case 23:
		case 24:
			DrawTable(rgba, id);
			break;
		case 25:
		case 26:
		case 27:
		case 28:
		case 29:
		case 30:
		case 31:
		case 32:
		case 33:
			DrawCarpet(rgba, id);
			break;
		default:
			ClearMagenta(rgba);
			break;
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
	constexpr uint16_t kCount = 33;

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
