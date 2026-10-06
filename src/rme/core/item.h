#pragma once

#include "definitions.h"
#include "position.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rme {
namespace core {

class Item {
public:
	explicit Item(uint16_t id = 0) :
		id_(id) { }

	uint16_t getID() const { return id_; }
	void setID(uint16_t id) { id_ = id; }

	uint8_t getCount() const { return count_; }
	void setCount(uint8_t count) { count_ = count; }

	uint16_t getActionID() const { return action_id_; }
	void setActionID(uint16_t id) { action_id_ = id; }

	uint16_t getUniqueID() const { return unique_id_; }
	void setUniqueID(uint16_t id) { unique_id_ = id; }

	uint16_t getCharges() const { return charges_; }
	void setCharges(uint16_t charges) { charges_ = charges; }

	const std::string& getText() const { return text_; }
	void setText(std::string text) { text_ = std::move(text); }

	const std::string& getDescription() const { return description_; }
	void setDescription(std::string description) { description_ = std::move(description); }

	bool hasDestination() const { return has_dest_; }
	Position getDestination() const { return dest_; }
	void setDestination(const Position& dest) {
		dest_ = dest;
		has_dest_ = true;
	}
	void clearDestination() {
		has_dest_ = false;
		dest_ = Position();
	}

	uint16_t getDepotID() const { return depot_id_; }
	void setDepotID(uint16_t id) { depot_id_ = id; }

	uint8_t getDoorID() const { return door_id_; }
	void setDoorID(uint8_t id) { door_id_ = id; }

	const std::vector<Item>& getContents() const { return contents_; }
	std::vector<Item>& getContents() { return contents_; }

	Item deepCopy() const { return *this; }

private:
	uint16_t id_ = 0;
	uint8_t count_ = 1;
	uint16_t action_id_ = 0;
	uint16_t unique_id_ = 0;
	uint16_t charges_ = 0;
	uint16_t depot_id_ = 0;
	uint8_t door_id_ = 0;
	bool has_dest_ = false;
	Position dest_;
	std::string text_;
	std::string description_;
	std::vector<Item> contents_;
};

} // namespace core
} // namespace rme
