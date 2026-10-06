#include "map_xml.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
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

bool ReadAll(const std::string& path, std::string& out) {
	std::ifstream in(path);
	if (!in) {
		return false;
	}
	std::ostringstream buffer;
	buffer << in.rdbuf();
	out = buffer.str();
	return true;
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
		const auto after = start + open.size();
		if (after < xml.size() && (std::isalnum(static_cast<unsigned char>(xml[after])) || xml[after] == '-' || xml[after] == '_')) {
			pos = after;
			continue;
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

std::string CompanionPath(const std::string& otbm_path, const std::string& filename, const std::string& fallback) {
	std::filesystem::path otbm(otbm_path);
	const std::string name = !filename.empty() ? filename : fallback;
	if (name.empty()) {
		return {};
	}
	return (otbm.parent_path() / name).string();
}

bool LoadHouseXml(Map& map, const std::string& path) {
	if (path.empty()) {
		return true;
	}
	std::string xml;
	if (!ReadAll(path, xml)) {
		return true;
	}
	map.houses().clear();
	std::size_t pos = 0;
	std::string tag;
	while (NextOpenTag(xml, "house", pos, tag)) {
		House house;
		house.id = static_cast<uint32_t>(AttrInt(tag, "houseid"));
		house.name = AttrString(tag, "name", "House");
		house.town_id = static_cast<uint32_t>(AttrInt(tag, "townid", 1));
		house.rent = static_cast<uint32_t>(AttrInt(tag, "rent"));
		house.entry = Position(AttrInt(tag, "entryx"), AttrInt(tag, "entryy"), AttrInt(tag, "entryz", rme::MapGroundLayer));
		if (house.id != 0) {
			map.houses().push_back(std::move(house));
		}
	}
	return true;
}

bool SaveHouseXml(const Map& map, const std::string& path) {
	if (path.empty()) {
		return true;
	}
	std::ofstream out(path, std::ios::trunc);
	if (!out) {
		return false;
	}
	out << "<?xml version=\"1.0\"?>\n<houses>\n";
	for (const House& house : map.houses()) {
		out << "\t<house name=\"" << XmlEscape(house.name) << "\" houseid=\"" << house.id
			<< "\" entryx=\"" << house.entry.x << "\" entryy=\"" << house.entry.y << "\" entryz=\"" << house.entry.z
			<< "\" rent=\"" << house.rent << "\" townid=\"" << house.town_id
			<< "\" size=\"" << map.houseTileCount(house.id) << "\" />\n";
	}
	out << "</houses>\n";
	return static_cast<bool>(out);
}

bool LoadSpawnXml(Map& map, const std::string& path) {
	if (path.empty()) {
		return true;
	}
	std::string xml;
	if (!ReadAll(path, xml)) {
		return true;
	}
	map.spawns().clear();
	std::size_t pos = 0;
	std::string tag;
	while (NextOpenTag(xml, "spawn", pos, tag)) {
		Spawn spawn;
		spawn.center = Position(
			AttrInt(tag, "centerx"),
			AttrInt(tag, "centery"),
			AttrInt(tag, "centerz", rme::MapGroundLayer)
		);
		spawn.radius = std::max(1, AttrInt(tag, "radius", 3));
		const auto close = xml.find("</spawn>", pos);
		const std::string body = xml.substr(pos, close == std::string::npos ? xml.size() - pos : close - pos);
		std::size_t inner = 0;
		std::string monster_tag;
		while (NextOpenTag(body, "monster", inner, monster_tag)) {
			SpawnCreature creature;
			creature.name = AttrString(monster_tag, "name", "Monster");
			creature.dx = AttrInt(monster_tag, "x");
			creature.dy = AttrInt(monster_tag, "y");
			creature.spawntime = static_cast<uint32_t>(std::max(1, AttrInt(monster_tag, "spawntime", 60)));
			spawn.monsters.push_back(std::move(creature));
		}
		map.spawns().push_back(std::move(spawn));
		if (close != std::string::npos) {
			pos = close + 8;
		}
	}
	return true;
}

bool SaveSpawnXml(const Map& map, const std::string& path) {
	if (path.empty()) {
		return true;
	}
	std::ofstream out(path, std::ios::trunc);
	if (!out) {
		return false;
	}
	out << "<?xml version=\"1.0\"?>\n<spawns>\n";
	for (const Spawn& spawn : map.spawns()) {
		out << "\t<spawn centerx=\"" << spawn.center.x << "\" centery=\"" << spawn.center.y
			<< "\" centerz=\"" << spawn.center.z << "\" radius=\"" << spawn.radius << "\">\n";
		for (const SpawnCreature& creature : spawn.monsters) {
			out << "\t\t<monster name=\"" << XmlEscape(creature.name) << "\" x=\"" << creature.dx
				<< "\" y=\"" << creature.dy << "\" z=\"" << spawn.center.z
				<< "\" spawntime=\"" << creature.spawntime << "\" />\n";
		}
		out << "\t</spawn>\n";
	}
	out << "</spawns>\n";
	return static_cast<bool>(out);
}

} // namespace core
} // namespace rme
