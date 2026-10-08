#include "materials.h"

#include <fstream>
#include <sstream>

namespace rme {
namespace core {
namespace {

std::string XmlEscape(const std::string& value) {
	std::string out;
	out.reserve(value.size());
	for (char ch : value) {
		switch (ch) {
			case '&':
				out += "&amp;";
				break;
			case '<':
				out += "&lt;";
				break;
			case '>':
				out += "&gt;";
				break;
			case '"':
				out += "&quot;";
				break;
			default:
				out += ch;
				break;
		}
	}
	return out;
}

std::string AttrString(const std::string& tag, const char* key, const std::string& fallback = {}) {
	const std::string needle = std::string(key) + "=\"";
	const auto start = tag.find(needle);
	if (start == std::string::npos) {
		return fallback;
	}
	const auto value_start = start + needle.size();
	const auto value_end = tag.find('"', value_start);
	if (value_end == std::string::npos) {
		return fallback;
	}
	return tag.substr(value_start, value_end - value_start);
}

int AttrInt(const std::string& tag, const char* key, int fallback = 0) {
	const std::string text = AttrString(tag, key);
	if (text.empty()) {
		return fallback;
	}
	try {
		return std::stoi(text);
	} catch (...) {
		return fallback;
	}
}

bool NextOpenTag(const std::string& xml, const char* name, std::size_t& pos, std::string& tag) {
	const std::string open = std::string("<") + name;
	while (true) {
		const auto start = xml.find(open, pos);
		if (start == std::string::npos) {
			return false;
		}
		if (start + open.size() < xml.size()) {
			const char next = xml[start + open.size()];
			if (next != ' ' && next != '>' && next != '/') {
				pos = start + 1;
				continue;
			}
		}
		const auto end = xml.find('>', start);
		if (end == std::string::npos) {
			return false;
		}
		tag = xml.substr(start, end - start + 1);
		pos = end + 1;
		return true;
	}
}

} // namespace

bool WallSet::contains(uint16_t item_id) const {
	if (item_id == 0) {
		return false;
	}
	if (look_id == item_id) {
		return true;
	}
	for (uint16_t piece_id : piece) {
		if (piece_id == item_id) {
			return true;
		}
	}
	return false;
}

uint16_t WallSet::pieceFor(uint8_t mask) const {
	const uint16_t id = piece[mask & 15];
	if (id != 0) {
		return id;
	}
	return look_id != 0 ? look_id : piece[0];
}

void WallSet::fillFromParts(uint16_t pole, uint16_t horizontal, uint16_t vertical, uint16_t junction) {
	look_id = pole;
	for (uint8_t mask = 0; mask < 16; ++mask) {
		const bool n = (mask & kWallNorth) != 0;
		const bool e = (mask & kWallEast) != 0;
		const bool s = (mask & kWallSouth) != 0;
		const bool w = (mask & kWallWest) != 0;
		const bool ns = n || s;
		const bool ew = e || w;
		if (ns && ew) {
			piece[mask] = junction;
		} else if (ns) {
			piece[mask] = vertical;
		} else if (ew) {
			piece[mask] = horizontal;
		} else {
			piece[mask] = pole;
		}
	}
}

void Materials::clear() {
	tilesets_.clear();
	walls_.clear();
	error_.clear();
}

void Materials::ensureDefaults() {
	if (!tilesets_.empty() || !walls_.empty()) {
		return;
	}
	WallSet timber;
	timber.name = "Timber";
	timber.fillFromParts(106, 107, 108, 109);
	walls_.push_back(timber);

	tilesets_.push_back(Tileset{"Grounds", {100, 101, 102}});
	tilesets_.push_back(Tileset{"Walls", {103, 106, 107, 108, 109}});
	tilesets_.push_back(Tileset{"Items", {104, 105}});
}

bool Materials::writeSample(const std::string& path) {
	ensureDefaults();
	std::ofstream out(path);
	if (!out) {
		error_ = "Could not write materials.xml";
		return false;
	}
	out << "<?xml version=\"1.0\"?>\n<materials>\n";
	for (const auto& set : tilesets_) {
		out << "  <tileset name=\"" << XmlEscape(set.name) << "\">\n";
		if (set.name == "Walls") {
			for (const auto& wall : walls_) {
				out << "    <wall name=\"" << XmlEscape(wall.name) << "\" lookid=\"" << wall.look_id
					<< "\" pole=\"" << wall.pieceFor(0) << "\" horizontal=\"" << wall.pieceFor(kWallEast | kWallWest)
					<< "\" vertical=\"" << wall.pieceFor(kWallNorth | kWallSouth) << "\" junction=\""
					<< wall.pieceFor(15) << "\"/>\n";
			}
		}
		for (uint16_t id : set.items) {
			out << "    <item id=\"" << id << "\"/>\n";
		}
		out << "  </tileset>\n";
	}
	out << "</materials>\n";
	error_.clear();
	return true;
}

bool Materials::load(const std::string& path) {
	std::ifstream in(path);
	if (!in) {
		error_ = "Could not open materials.xml";
		return false;
	}
	std::ostringstream buffer;
	buffer << in.rdbuf();
	const std::string xml = buffer.str();

	clear();
	std::size_t pos = 0;
	std::string tag;
	Tileset* current = nullptr;
	std::size_t tileset_end = std::string::npos;
	while (true) {
		if (current && tileset_end != std::string::npos && pos >= tileset_end) {
			current = nullptr;
			tileset_end = std::string::npos;
		}
		const auto tileset_at = xml.find("<tileset", pos);
		const auto item_at = xml.find("<item", pos);
		const auto wall_at = xml.find("<wall", pos);
		std::size_t next = std::string::npos;
		const char* kind = nullptr;
		if (tileset_at != std::string::npos && (next == std::string::npos || tileset_at < next)) {
			next = tileset_at;
			kind = "tileset";
		}
		if (item_at != std::string::npos && (next == std::string::npos || item_at < next)) {
			next = item_at;
			kind = "item";
		}
		if (wall_at != std::string::npos && (next == std::string::npos || wall_at < next)) {
			next = wall_at;
			kind = "wall";
		}
		if (next == std::string::npos) {
			break;
		}
		pos = next;
		if (!NextOpenTag(xml, kind, pos, tag)) {
			break;
		}
		if (std::string(kind) == "tileset") {
			Tileset set;
			set.name = AttrString(tag, "name", "Tileset");
			tilesets_.push_back(std::move(set));
			current = &tilesets_.back();
			tileset_end = xml.find("</tileset>", pos);
		} else if (std::string(kind) == "item") {
			const int id = AttrInt(tag, "id");
			if (id > 0 && current) {
				current->items.push_back(static_cast<uint16_t>(id));
			}
		} else if (std::string(kind) == "wall") {
			WallSet wall;
			wall.name = AttrString(tag, "name", "Wall");
			const uint16_t pole = static_cast<uint16_t>(AttrInt(tag, "pole", AttrInt(tag, "lookid")));
			const uint16_t horizontal = static_cast<uint16_t>(AttrInt(tag, "horizontal", pole));
			const uint16_t vertical = static_cast<uint16_t>(AttrInt(tag, "vertical", pole));
			const uint16_t junction = static_cast<uint16_t>(AttrInt(tag, "junction", pole));
			wall.fillFromParts(pole, horizontal, vertical, junction);
			wall.look_id = static_cast<uint16_t>(AttrInt(tag, "lookid", pole));
			walls_.push_back(std::move(wall));
		}
	}

	if (tilesets_.empty() && walls_.empty()) {
		error_ = "materials.xml had no tilesets";
		return false;
	}
	error_.clear();
	return true;
}

const WallSet* Materials::wallForItem(uint16_t item_id) const {
	for (const auto& wall : walls_) {
		if (wall.contains(item_id)) {
			return &wall;
		}
	}
	return nullptr;
}

bool TileHasWall(const Tile& tile, const WallSet& set) {
	for (const Item& item : tile.getItems()) {
		if (set.contains(item.getID())) {
			return true;
		}
	}
	return false;
}

uint8_t WallNeighborMask(const Map& map, const Position& position, const WallSet& set) {
	uint8_t mask = 0;
	const Position north(position.x, position.y - 1, position.z);
	const Position east(position.x + 1, position.y, position.z);
	const Position south(position.x, position.y + 1, position.z);
	const Position west(position.x - 1, position.y, position.z);
	if (const Tile* tile = map.getTile(north); tile && TileHasWall(*tile, set)) {
		mask |= kWallNorth;
	}
	if (const Tile* tile = map.getTile(east); tile && TileHasWall(*tile, set)) {
		mask |= kWallEast;
	}
	if (const Tile* tile = map.getTile(south); tile && TileHasWall(*tile, set)) {
		mask |= kWallSouth;
	}
	if (const Tile* tile = map.getTile(west); tile && TileHasWall(*tile, set)) {
		mask |= kWallWest;
	}
	return mask;
}

uint16_t ResolveWallPiece(const Map& map, const Position& position, const WallSet& set) {
	return set.pieceFor(WallNeighborMask(map, position, set));
}

bool ApplyWallToTile(Tile& tile, uint16_t piece_id, const WallSet& set) {
	bool changed = false;
	bool has_piece = false;
	auto& items = tile.getItems();
	for (auto it = items.begin(); it != items.end();) {
		if (!set.contains(it->getID())) {
			++it;
			continue;
		}
		if (!has_piece && it->getID() == piece_id) {
			has_piece = true;
			++it;
			continue;
		}
		it = items.erase(it);
		changed = true;
	}
	if (!has_piece) {
		tile.addItem(Item(piece_id));
		changed = true;
	}
	return changed;
}

bool RemoveWallFromTile(Tile& tile, const WallSet& set) {
	bool changed = false;
	auto& items = tile.getItems();
	for (auto it = items.begin(); it != items.end();) {
		if (set.contains(it->getID())) {
			it = items.erase(it);
			changed = true;
		} else {
			++it;
		}
	}
	return changed;
}

} // namespace core
} // namespace rme
