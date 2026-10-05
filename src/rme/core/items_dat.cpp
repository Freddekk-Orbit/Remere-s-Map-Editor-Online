#include "items_dat.h"

#include "filehandle.h"

namespace rme {
namespace core {

bool LoadDatHeader(const std::string& path, ClientAssetsInfo& info) {
	FileReadHandle file(path);
	if (!file.isOk()) {
		info.error = file.getErrorMessage();
		info.dat_loaded = false;
		return false;
	}

	if (!file.getU32(info.dat_signature)) {
		info.error = "Could not read .dat signature";
		return false;
	}

	uint16_t items16 = 0, outfits16 = 0, effects16 = 0, missiles16 = 0;
	if (!file.getU16(items16) || !file.getU16(outfits16) || !file.getU16(effects16) || !file.getU16(missiles16)) {
		info.error = "Could not read .dat object counts";
		return false;
	}

	info.item_count = items16;
	info.outfit_count = outfits16;
	info.effect_count = effects16;
	info.missile_count = missiles16;
	info.dat_path = path;
	info.dat_loaded = true;
	info.error.clear();
	return true;
}

bool LoadSprHeader(const std::string& path, ClientAssetsInfo& info) {
	FileReadHandle file(path);
	if (!file.isOk()) {
		info.error = file.getErrorMessage();
		info.spr_loaded = false;
		return false;
	}

	if (!file.getU32(info.spr_signature)) {
		info.error = "Could not read .spr signature";
		return false;
	}

	uint32_t count32 = 0;
	uint16_t count16 = 0;
	// 10.50+ stores a 32-bit sprite count; older files use 16-bit.
	const auto remaining = file.size() > 4 ? file.size() - 4 : 0;
	if (remaining >= 4 && file.getU32(count32) && count32 > 0 && count32 < 2'000'000) {
		info.sprite_count = count32;
	} else {
		file.seek(4);
		if (!file.getU16(count16)) {
			info.error = "Could not read .spr sprite count";
			return false;
		}
		info.sprite_count = count16;
	}

	info.spr_path = path;
	info.spr_loaded = true;
	info.error.clear();
	return true;
}

} // namespace core
} // namespace rme
