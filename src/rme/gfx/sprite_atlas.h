#pragma once

#include "rme/core/sprites.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace rme {
namespace gfx {

struct SpriteUv {
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;
};

// Packs CPU sprites into one GL_RGBA / NEAREST atlas. Keep GL out of src/rme/core.
class SpriteAtlas {
public:
	SpriteAtlas() = default;
	~SpriteAtlas() { destroy(); }

	SpriteAtlas(const SpriteAtlas&) = delete;
	SpriteAtlas& operator=(const SpriteAtlas&) = delete;

	bool upload(const core::SpriteSheet& sheet);
	void destroy();

	bool valid() const { return texture_ != 0; }
	unsigned texture() const { return texture_; }
	int size() const { return atlas_size_; }
	bool has(uint32_t sprite_id) const;
	SpriteUv uv(uint32_t sprite_id) const;

private:
	unsigned texture_ = 0;
	int atlas_size_ = 0;
	int per_row_ = 0;
	std::unordered_map<uint32_t, SpriteUv> uvs_;
};

} // namespace gfx
} // namespace rme
