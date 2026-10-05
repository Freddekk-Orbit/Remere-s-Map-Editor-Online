#include "sprite_atlas.h"

#ifdef __EMSCRIPTEN__
	#include <GLES3/gl3.h>
#else
	#include <SDL_opengl.h>
#endif

#include <algorithm>
#include <cmath>

namespace rme {
namespace gfx {
namespace {

int NextAtlasSize(int sprite_count) {
	if (sprite_count <= 0) {
		return 0;
	}
	const int min_dim = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(sprite_count)))) * core::kSpriteSize;
	int size = 256;
	while (size < min_dim && size < 2048) {
		size *= 2;
	}
	return std::max(256, size);
}

void BlitSprite(std::vector<uint8_t>& atlas, int atlas_size, int dest_x, int dest_y, const core::Sprite& sprite) {
	const int src_w = sprite.width > 0 ? sprite.width : core::kSpriteSize;
	const int src_h = sprite.height > 0 ? sprite.height : core::kSpriteSize;
	for (int y = 0; y < src_h && dest_y + y < atlas_size; ++y) {
		for (int x = 0; x < src_w && dest_x + x < atlas_size; ++x) {
			const int src = (y * src_w + x) * 4;
			const int dst = ((dest_y + y) * atlas_size + (dest_x + x)) * 4;
			if (src + 3 >= static_cast<int>(sprite.rgba.size())) {
				continue;
			}
			atlas[static_cast<std::size_t>(dst) + 0] = sprite.rgba[static_cast<std::size_t>(src) + 0];
			atlas[static_cast<std::size_t>(dst) + 1] = sprite.rgba[static_cast<std::size_t>(src) + 1];
			atlas[static_cast<std::size_t>(dst) + 2] = sprite.rgba[static_cast<std::size_t>(src) + 2];
			atlas[static_cast<std::size_t>(dst) + 3] = sprite.rgba[static_cast<std::size_t>(src) + 3];
		}
	}
}

} // namespace

bool SpriteAtlas::has(uint32_t sprite_id) const {
	return uvs_.find(sprite_id) != uvs_.end();
}

SpriteUv SpriteAtlas::uv(uint32_t sprite_id) const {
	const auto it = uvs_.find(sprite_id);
	if (it == uvs_.end()) {
		return {};
	}
	return it->second;
}

void SpriteAtlas::destroy() {
	if (texture_ != 0) {
		glDeleteTextures(1, &texture_);
		texture_ = 0;
	}
	atlas_size_ = 0;
	per_row_ = 0;
	uvs_.clear();
}

bool SpriteAtlas::upload(const core::SpriteSheet& sheet) {
	destroy();

	int used = 0;
	for (const auto& sprite : sheet.sprites()) {
		if (!sprite.empty()) {
			++used;
		}
	}
	if (used == 0) {
		return false;
	}

	atlas_size_ = NextAtlasSize(used);
	per_row_ = atlas_size_ / core::kSpriteSize;
	if (per_row_ <= 0) {
		return false;
	}

	std::vector<uint8_t> pixels(static_cast<std::size_t>(atlas_size_ * atlas_size_ * 4), 0);
	int slot = 0;
	for (const auto& sprite : sheet.sprites()) {
		if (sprite.empty()) {
			continue;
		}
		const int col = slot % per_row_;
		const int row = slot / per_row_;
		const int x = col * core::kSpriteSize;
		const int y = row * core::kSpriteSize;
		BlitSprite(pixels, atlas_size_, x, y, sprite);

		SpriteUv uv;
		uv.u0 = static_cast<float>(x) / static_cast<float>(atlas_size_);
		uv.v0 = static_cast<float>(y) / static_cast<float>(atlas_size_);
		uv.u1 = static_cast<float>(x + core::kSpriteSize) / static_cast<float>(atlas_size_);
		uv.v1 = static_cast<float>(y + core::kSpriteSize) / static_cast<float>(atlas_size_);
		uvs_[sprite.id] = uv;
		++slot;
	}

	glGenTextures(1, &texture_);
	glBindTexture(GL_TEXTURE_2D, texture_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_size_, atlas_size_, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glBindTexture(GL_TEXTURE_2D, 0);
	return texture_ != 0;
}

} // namespace gfx
} // namespace rme
