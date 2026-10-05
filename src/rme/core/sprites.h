#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rme {
namespace core {

constexpr int kSpriteSize = 32;

struct Sprite {
	uint32_t id = 0;
	int width = kSpriteSize;
	int height = kSpriteSize;
	std::vector<uint8_t> rgba; // width * height * 4
	bool empty() const { return rgba.empty(); }
};

class SpriteSheet {
public:
	void clear();
	bool load(const std::string& path, uint32_t& signature, uint32_t& count, std::string& error);
	bool writeSample(const std::string& path);

	const Sprite* get(uint32_t id) const;
	const std::vector<Sprite>& sprites() const { return sprites_; }
	uint32_t count() const { return count_; }

	static std::vector<uint8_t> MakeSolidSprite(uint8_t r, uint8_t g, uint8_t b, uint8_t border = 24);
	static bool DecodeSprite(const uint8_t* data, std::size_t size, Sprite& out);
	static std::vector<uint8_t> EncodeSprite(const std::vector<uint8_t>& rgba);

private:
	std::vector<Sprite> sprites_;
	uint32_t count_ = 0;
};

} // namespace core
} // namespace rme
