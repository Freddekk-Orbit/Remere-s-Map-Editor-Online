#include "materials.h"

#include <algorithm>
#include <fstream>
#include <iterator>
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

std::vector<uint16_t> ParseIdList(const std::string& text) {
	std::vector<uint16_t> ids;
	std::size_t i = 0;
	while (i < text.size()) {
		while (i < text.size() && (text[i] == ',' || text[i] == ' ')) {
			++i;
		}
		if (i >= text.size()) {
			break;
		}
		std::size_t j = i;
		while (j < text.size() && text[j] != ',' && text[j] != ' ') {
			++j;
		}
		try {
			const int id = std::stoi(text.substr(i, j - i));
			if (id > 0) {
				ids.push_back(static_cast<uint16_t>(id));
			}
		} catch (...) {
		}
		i = j;
	}
	return ids;
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

bool GroundBorderSet::containsBorder(uint16_t item_id) const {
	if (item_id == 0) {
		return false;
	}
	return item_id == edge_n || item_id == edge_e || item_id == edge_s || item_id == edge_w
		|| item_id == corner_ne || item_id == corner_se || item_id == corner_sw || item_id == corner_nw;
}

bool DoodadSet::contains(uint16_t item_id) const {
	if (item_id == 0) {
		return false;
	}
	if (look_id == item_id) {
		return true;
	}
	for (uint16_t id : items) {
		if (id == item_id) {
			return true;
		}
	}
	return false;
}

uint16_t DoodadSet::pick(const Position& position) const {
	if (items.empty()) {
		return look_id;
	}
	const std::size_t index = static_cast<std::size_t>(position.x + position.y + position.z) % items.size();
	return items[index];
}

bool DoorSet::contains(uint16_t item_id) const {
	return item_id != 0 && (item_id == look_id || item_id == horizontal || item_id == vertical);
}

uint16_t DoorSet::pieceFor(bool horizontal_door) const {
	const uint16_t id = horizontal_door ? horizontal : vertical;
	if (id != 0) {
		return id;
	}
	return look_id;
}

bool TableSet::contains(uint16_t item_id) const {
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

uint16_t TableSet::pieceFor(uint8_t mask) const {
	const uint16_t id = piece[mask & 15];
	if (id != 0) {
		return id;
	}
	return look_id != 0 ? look_id : piece[0];
}

void TableSet::fillFromParts(uint16_t pole, uint16_t horizontal, uint16_t vertical, uint16_t junction) {
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

bool CarpetSet::contains(uint16_t item_id) const {
	if (item_id == 0) {
		return false;
	}
	return item_id == look_id || item_id == inner_id || item_id == edge_n || item_id == edge_e || item_id == edge_s
		|| item_id == edge_w || item_id == corner_ne || item_id == corner_se || item_id == corner_sw
		|| item_id == corner_nw;
}

uint16_t CarpetSet::pieceFor(bool n, bool e, bool s, bool w) const {
	if (n && e && corner_ne != 0) {
		return corner_ne;
	}
	if (e && s && corner_se != 0) {
		return corner_se;
	}
	if (s && w && corner_sw != 0) {
		return corner_sw;
	}
	if (w && n && corner_nw != 0) {
		return corner_nw;
	}
	if (n && edge_n != 0) {
		return edge_n;
	}
	if (e && edge_e != 0) {
		return edge_e;
	}
	if (s && edge_s != 0) {
		return edge_s;
	}
	if (w && edge_w != 0) {
		return edge_w;
	}
	return inner_id != 0 ? inner_id : look_id;
}

void Materials::clear() {
	tilesets_.clear();
	walls_.clear();
	borders_.clear();
	doodads_.clear();
	doors_.clear();
	tables_.clear();
	carpets_.clear();
	error_.clear();
}

void Materials::ensureDefaults() {
	if (!tilesets_.empty() || !walls_.empty() || !borders_.empty() || !doodads_.empty() || !doors_.empty()
		|| !tables_.empty() || !carpets_.empty()) {
		return;
	}
	WallSet timber;
	timber.name = "Timber";
	timber.fillFromParts(106, 107, 108, 109);
	walls_.push_back(timber);

	GroundBorderSet water;
	water.name = "Water";
	water.inner_id = 102;
	water.edge_n = 110;
	water.edge_e = 111;
	water.edge_s = 112;
	water.edge_w = 113;
	water.corner_ne = 114;
	water.corner_se = 115;
	water.corner_sw = 116;
	water.corner_nw = 117;
	borders_.push_back(water);

	DoodadSet flowers;
	flowers.name = "Flowers";
	flowers.look_id = 104;
	flowers.chance = 60;
	flowers.items = {104};
	doodads_.push_back(flowers);

	DoorSet wood_door;
	wood_door.name = "Wood";
	wood_door.look_id = 118;
	wood_door.horizontal = 118;
	wood_door.vertical = 119;
	doors_.push_back(wood_door);

	TableSet wood_table;
	wood_table.name = "Wood";
	wood_table.fillFromParts(120, 121, 122, 123);
	tables_.push_back(wood_table);

	CarpetSet red_carpet;
	red_carpet.name = "Red";
	red_carpet.look_id = 124;
	red_carpet.inner_id = 124;
	red_carpet.edge_n = 125;
	red_carpet.edge_e = 126;
	red_carpet.edge_s = 127;
	red_carpet.edge_w = 128;
	red_carpet.corner_ne = 129;
	red_carpet.corner_se = 130;
	red_carpet.corner_sw = 131;
	red_carpet.corner_nw = 132;
	carpets_.push_back(red_carpet);

	tilesets_.push_back(Tileset{"Grounds", {100, 101, 102}});
	tilesets_.push_back(Tileset{"Borders", {110, 111, 112, 113, 114, 115, 116, 117}});
	tilesets_.push_back(Tileset{"Walls", {103, 106, 107, 108, 109}});
	tilesets_.push_back(Tileset{"Items", {104, 105}});
	tilesets_.push_back(Tileset{"Furniture", {118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132}});
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
		if (set.name == "Grounds") {
			for (const auto& border : borders_) {
				out << "    <ground name=\"" << XmlEscape(border.name) << "\" lookid=\"" << border.inner_id
					<< "\" inner=\"" << border.inner_id << "\" edge_n=\"" << border.edge_n << "\" edge_e=\""
					<< border.edge_e << "\" edge_s=\"" << border.edge_s << "\" edge_w=\"" << border.edge_w
					<< "\" corner_ne=\"" << border.corner_ne << "\" corner_se=\"" << border.corner_se
					<< "\" corner_sw=\"" << border.corner_sw << "\" corner_nw=\"" << border.corner_nw << "\"/>\n";
			}
		}
		if (set.name == "Items") {
			for (const auto& doodad : doodads_) {
				out << "    <doodad name=\"" << XmlEscape(doodad.name) << "\" lookid=\"" << doodad.look_id
					<< "\" chance=\"" << doodad.chance << "\" items=\"";
				for (std::size_t i = 0; i < doodad.items.size(); ++i) {
					if (i > 0) {
						out << ",";
					}
					out << doodad.items[i];
				}
				out << "\"/>\n";
			}
		}
		if (set.name == "Furniture") {
			for (const auto& door : doors_) {
				out << "    <door name=\"" << XmlEscape(door.name) << "\" lookid=\"" << door.look_id
					<< "\" horizontal=\"" << door.horizontal << "\" vertical=\"" << door.vertical << "\"/>\n";
			}
			for (const auto& table : tables_) {
				out << "    <table name=\"" << XmlEscape(table.name) << "\" lookid=\"" << table.look_id
					<< "\" pole=\"" << table.pieceFor(0) << "\" horizontal=\"" << table.pieceFor(kWallEast | kWallWest)
					<< "\" vertical=\"" << table.pieceFor(kWallNorth | kWallSouth) << "\" junction=\""
					<< table.pieceFor(15) << "\"/>\n";
			}
			for (const auto& carpet : carpets_) {
				out << "    <carpet name=\"" << XmlEscape(carpet.name) << "\" lookid=\"" << carpet.look_id
					<< "\" inner=\"" << carpet.inner_id << "\" edge_n=\"" << carpet.edge_n << "\" edge_e=\""
					<< carpet.edge_e << "\" edge_s=\"" << carpet.edge_s << "\" edge_w=\"" << carpet.edge_w
					<< "\" corner_ne=\"" << carpet.corner_ne << "\" corner_se=\"" << carpet.corner_se
					<< "\" corner_sw=\"" << carpet.corner_sw << "\" corner_nw=\"" << carpet.corner_nw << "\"/>\n";
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
		const auto ground_at = xml.find("<ground", pos);
		const auto doodad_at = xml.find("<doodad", pos);
		const auto door_at = xml.find("<door", pos);
		const auto table_at = xml.find("<table", pos);
		const auto carpet_at = xml.find("<carpet", pos);
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
		if (ground_at != std::string::npos && (next == std::string::npos || ground_at < next)) {
			next = ground_at;
			kind = "ground";
		}
		if (doodad_at != std::string::npos && (next == std::string::npos || doodad_at < next)) {
			next = doodad_at;
			kind = "doodad";
		}
		if (door_at != std::string::npos && (next == std::string::npos || door_at < next)) {
			next = door_at;
			kind = "door";
		}
		if (table_at != std::string::npos && (next == std::string::npos || table_at < next)) {
			next = table_at;
			kind = "table";
		}
		if (carpet_at != std::string::npos && (next == std::string::npos || carpet_at < next)) {
			next = carpet_at;
			kind = "carpet";
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
		} else if (std::string(kind) == "ground") {
			GroundBorderSet border;
			border.name = AttrString(tag, "name", "Ground");
			border.inner_id = static_cast<uint16_t>(AttrInt(tag, "inner", AttrInt(tag, "lookid")));
			border.edge_n = static_cast<uint16_t>(AttrInt(tag, "edge_n"));
			border.edge_e = static_cast<uint16_t>(AttrInt(tag, "edge_e"));
			border.edge_s = static_cast<uint16_t>(AttrInt(tag, "edge_s"));
			border.edge_w = static_cast<uint16_t>(AttrInt(tag, "edge_w"));
			border.corner_ne = static_cast<uint16_t>(AttrInt(tag, "corner_ne"));
			border.corner_se = static_cast<uint16_t>(AttrInt(tag, "corner_se"));
			border.corner_sw = static_cast<uint16_t>(AttrInt(tag, "corner_sw"));
			border.corner_nw = static_cast<uint16_t>(AttrInt(tag, "corner_nw"));
			borders_.push_back(std::move(border));
		} else if (std::string(kind) == "doodad") {
			DoodadSet doodad;
			doodad.name = AttrString(tag, "name", "Doodad");
			doodad.look_id = static_cast<uint16_t>(AttrInt(tag, "lookid"));
			doodad.chance = AttrInt(tag, "chance", 60);
			doodad.items = ParseIdList(AttrString(tag, "items"));
			if (doodad.items.empty() && doodad.look_id != 0) {
				doodad.items.push_back(doodad.look_id);
			}
			doodads_.push_back(std::move(doodad));
		} else if (std::string(kind) == "door") {
			DoorSet door;
			door.name = AttrString(tag, "name", "Door");
			door.look_id = static_cast<uint16_t>(AttrInt(tag, "lookid"));
			door.horizontal = static_cast<uint16_t>(AttrInt(tag, "horizontal", door.look_id));
			door.vertical = static_cast<uint16_t>(AttrInt(tag, "vertical", door.look_id));
			doors_.push_back(std::move(door));
		} else if (std::string(kind) == "table") {
			TableSet table;
			table.name = AttrString(tag, "name", "Table");
			const uint16_t pole = static_cast<uint16_t>(AttrInt(tag, "pole", AttrInt(tag, "lookid")));
			const uint16_t horizontal = static_cast<uint16_t>(AttrInt(tag, "horizontal", pole));
			const uint16_t vertical = static_cast<uint16_t>(AttrInt(tag, "vertical", pole));
			const uint16_t junction = static_cast<uint16_t>(AttrInt(tag, "junction", pole));
			table.fillFromParts(pole, horizontal, vertical, junction);
			table.look_id = static_cast<uint16_t>(AttrInt(tag, "lookid", pole));
			tables_.push_back(std::move(table));
		} else if (std::string(kind) == "carpet") {
			CarpetSet carpet;
			carpet.name = AttrString(tag, "name", "Carpet");
			carpet.look_id = static_cast<uint16_t>(AttrInt(tag, "lookid"));
			carpet.inner_id = static_cast<uint16_t>(AttrInt(tag, "inner", carpet.look_id));
			carpet.edge_n = static_cast<uint16_t>(AttrInt(tag, "edge_n"));
			carpet.edge_e = static_cast<uint16_t>(AttrInt(tag, "edge_e"));
			carpet.edge_s = static_cast<uint16_t>(AttrInt(tag, "edge_s"));
			carpet.edge_w = static_cast<uint16_t>(AttrInt(tag, "edge_w"));
			carpet.corner_ne = static_cast<uint16_t>(AttrInt(tag, "corner_ne"));
			carpet.corner_se = static_cast<uint16_t>(AttrInt(tag, "corner_se"));
			carpet.corner_sw = static_cast<uint16_t>(AttrInt(tag, "corner_sw"));
			carpet.corner_nw = static_cast<uint16_t>(AttrInt(tag, "corner_nw"));
			carpets_.push_back(std::move(carpet));
		}
	}

	if (tilesets_.empty() && walls_.empty() && borders_.empty() && doodads_.empty() && doors_.empty()
		&& tables_.empty() && carpets_.empty()) {
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

const GroundBorderSet* Materials::borderForItem(uint16_t item_id) const {
	for (const auto& border : borders_) {
		if (border.contains(item_id)) {
			return &border;
		}
	}
	return nullptr;
}

const DoodadSet* Materials::doodadForItem(uint16_t item_id) const {
	for (const auto& doodad : doodads_) {
		if (doodad.contains(item_id)) {
			return &doodad;
		}
	}
	return nullptr;
}

const DoorSet* Materials::doorForItem(uint16_t item_id) const {
	for (const auto& door : doors_) {
		if (door.contains(item_id)) {
			return &door;
		}
	}
	return nullptr;
}

const TableSet* Materials::tableForItem(uint16_t item_id) const {
	for (const auto& table : tables_) {
		if (table.contains(item_id)) {
			return &table;
		}
	}
	return nullptr;
}

const CarpetSet* Materials::carpetForItem(uint16_t item_id) const {
	for (const auto& carpet : carpets_) {
		if (carpet.contains(item_id)) {
			return &carpet;
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

bool TileHasInnerGround(const Tile& tile, const GroundBorderSet& set) {
	return tile.hasGround() && set.containsInner(tile.getGround()->getID());
}

std::vector<uint16_t> ResolveBorderPieces(const Map& map, const Position& position, const GroundBorderSet& set) {
	std::vector<uint16_t> pieces;
	const Tile* self = map.getTile(position);
	if (!self || !TileHasInnerGround(*self, set)) {
		return pieces;
	}

	auto same = [&](int dx, int dy) {
		const Position next(position.x + dx, position.y + dy, position.z);
		const Tile* tile = map.getTile(next);
		return tile && TileHasInnerGround(*tile, set);
	};
	const bool n = !same(0, -1);
	const bool e = !same(1, 0);
	const bool s = !same(0, 1);
	const bool w = !same(-1, 0);

	if (n && e && set.corner_ne != 0) {
		pieces.push_back(set.corner_ne);
	}
	if (e && s && set.corner_se != 0) {
		pieces.push_back(set.corner_se);
	}
	if (s && w && set.corner_sw != 0) {
		pieces.push_back(set.corner_sw);
	}
	if (w && n && set.corner_nw != 0) {
		pieces.push_back(set.corner_nw);
	}
	if (n && !e && !w && set.edge_n != 0) {
		pieces.push_back(set.edge_n);
	}
	if (e && !n && !s && set.edge_e != 0) {
		pieces.push_back(set.edge_e);
	}
	if (s && !e && !w && set.edge_s != 0) {
		pieces.push_back(set.edge_s);
	}
	if (w && !n && !s && set.edge_w != 0) {
		pieces.push_back(set.edge_w);
	}
	return pieces;
}

bool RemoveBordersFromTile(Tile& tile, const GroundBorderSet& set) {
	bool changed = false;
	auto& items = tile.getItems();
	for (auto it = items.begin(); it != items.end();) {
		if (set.containsBorder(it->getID())) {
			it = items.erase(it);
			changed = true;
		} else {
			++it;
		}
	}
	return changed;
}

bool ApplyBordersToTile(Tile& tile, const Map& map, const GroundBorderSet& set) {
	const auto wanted = TileHasInnerGround(tile, set) ? ResolveBorderPieces(map, tile.getPosition(), set)
													 : std::vector<uint16_t>{};
	std::vector<uint16_t> have;
	for (const Item& item : tile.getItems()) {
		if (set.containsBorder(item.getID())) {
			have.push_back(item.getID());
		}
	}
	if (have == wanted) {
		return false;
	}
	RemoveBordersFromTile(tile, set);
	auto& items = tile.getItems();
	std::vector<Item> borders;
	borders.reserve(wanted.size());
	for (uint16_t id : wanted) {
		borders.emplace_back(id);
	}
	items.insert(items.begin(), std::make_move_iterator(borders.begin()), std::make_move_iterator(borders.end()));
	return true;
}

bool DoodadHits(const Position& position, int chance) {
	chance = std::clamp(chance, 0, 100);
	if (chance >= 100) {
		return true;
	}
	if (chance <= 0) {
		return false;
	}
	const uint32_t roll = static_cast<uint32_t>(position.x * 17 + position.y * 31 + position.z * 13) % 100u;
	return roll < static_cast<uint32_t>(chance);
}

bool ApplyDoodadToTile(Tile& tile, uint16_t item_id) {
	if (item_id == 0) {
		return false;
	}
	for (const Item& item : tile.getItems()) {
		if (item.getID() == item_id) {
			return false;
		}
	}
	tile.addItem(Item(item_id));
	return true;
}

bool RemoveDoodadFromTile(Tile& tile, const DoodadSet& set) {
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

namespace {

template <typename Contains>
bool ReplaceFamilyOnTile(Tile& tile, uint16_t piece_id, Contains contains) {
	if (piece_id == 0) {
		return false;
	}
	bool changed = false;
	bool has_piece = false;
	auto& items = tile.getItems();
	for (auto it = items.begin(); it != items.end();) {
		if (!contains(*it)) {
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

template <typename Contains>
bool EraseFamilyFromTile(Tile& tile, Contains contains) {
	bool changed = false;
	auto& items = tile.getItems();
	for (auto it = items.begin(); it != items.end();) {
		if (contains(*it)) {
			it = items.erase(it);
			changed = true;
		} else {
			++it;
		}
	}
	return changed;
}

} // namespace

bool TileHasDoor(const Tile& tile, const DoorSet& set) {
	for (const Item& item : tile.getItems()) {
		if (set.contains(item.getID())) {
			return true;
		}
	}
	return false;
}

uint16_t ResolveDoorPiece(const Map& map, const Position& position, const DoorSet& set, const WallSet* walls) {
	bool ew = false;
	bool ns = false;
	if (walls) {
		const uint8_t mask = WallNeighborMask(map, position, *walls);
		ew = (mask & (kWallEast | kWallWest)) != 0;
		ns = (mask & (kWallNorth | kWallSouth)) != 0;
		if (const Tile* self = map.getTile(position); self && TileHasWall(*self, *walls)) {
			if (!ew && !ns) {
				const uint16_t piece = ResolveWallPiece(map, position, *walls);
				if (piece == walls->pieceFor(kWallEast | kWallWest)) {
					ew = true;
				} else if (piece == walls->pieceFor(kWallNorth | kWallSouth)) {
					ns = true;
				}
			}
		}
	}
	if (ew && !ns) {
		return set.pieceFor(true);
	}
	if (ns && !ew) {
		return set.pieceFor(false);
	}
	return set.pieceFor(ew);
}

bool ApplyDoorToTile(Tile& tile, uint16_t piece_id, const DoorSet& set) {
	return ReplaceFamilyOnTile(tile, piece_id, [&](const Item& item) { return set.contains(item.getID()); });
}

bool RemoveDoorFromTile(Tile& tile, const DoorSet& set) {
	return EraseFamilyFromTile(tile, [&](const Item& item) { return set.contains(item.getID()); });
}

bool TileHasTable(const Tile& tile, const TableSet& set) {
	for (const Item& item : tile.getItems()) {
		if (set.contains(item.getID())) {
			return true;
		}
	}
	return false;
}

uint8_t TableNeighborMask(const Map& map, const Position& position, const TableSet& set) {
	uint8_t mask = 0;
	const Position north(position.x, position.y - 1, position.z);
	const Position east(position.x + 1, position.y, position.z);
	const Position south(position.x, position.y + 1, position.z);
	const Position west(position.x - 1, position.y, position.z);
	if (const Tile* tile = map.getTile(north); tile && TileHasTable(*tile, set)) {
		mask |= kWallNorth;
	}
	if (const Tile* tile = map.getTile(east); tile && TileHasTable(*tile, set)) {
		mask |= kWallEast;
	}
	if (const Tile* tile = map.getTile(south); tile && TileHasTable(*tile, set)) {
		mask |= kWallSouth;
	}
	if (const Tile* tile = map.getTile(west); tile && TileHasTable(*tile, set)) {
		mask |= kWallWest;
	}
	return mask;
}

uint16_t ResolveTablePiece(const Map& map, const Position& position, const TableSet& set) {
	return set.pieceFor(TableNeighborMask(map, position, set));
}

bool ApplyTableToTile(Tile& tile, uint16_t piece_id, const TableSet& set) {
	return ReplaceFamilyOnTile(tile, piece_id, [&](const Item& item) { return set.contains(item.getID()); });
}

bool RemoveTableFromTile(Tile& tile, const TableSet& set) {
	return EraseFamilyFromTile(tile, [&](const Item& item) { return set.contains(item.getID()); });
}

bool TileHasCarpet(const Tile& tile, const CarpetSet& set) {
	for (const Item& item : tile.getItems()) {
		if (set.contains(item.getID())) {
			return true;
		}
	}
	return false;
}

uint16_t ResolveCarpetPiece(const Map& map, const Position& position, const CarpetSet& set) {
	auto same = [&](int dx, int dy) {
		const Position next(position.x + dx, position.y + dy, position.z);
		const Tile* tile = map.getTile(next);
		return tile && TileHasCarpet(*tile, set);
	};
	return set.pieceFor(!same(0, -1), !same(1, 0), !same(0, 1), !same(-1, 0));
}

bool ApplyCarpetToTile(Tile& tile, uint16_t piece_id, const CarpetSet& set) {
	return ReplaceFamilyOnTile(tile, piece_id, [&](const Item& item) { return set.contains(item.getID()); });
}

bool RemoveCarpetFromTile(Tile& tile, const CarpetSet& set) {
	return EraseFamilyFromTile(tile, [&](const Item& item) { return set.contains(item.getID()); });
}

} // namespace core
} // namespace rme
