#pragma once

#include "const.h"
#include "item.h"
#include "position.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace rme {
namespace core {

enum TileState : uint32_t {
	TILESTATE_NONE = 0x0000,
	TILESTATE_PROTECTIONZONE = 0x0001,
	TILESTATE_NOPVP = 0x0004,
	TILESTATE_NOLOGOUT = 0x0008,
	TILESTATE_PVPZONE = 0x0010,
	TILESTATE_REFRESH = 0x0020,
};

class Tile {
public:
	Tile() = default;
	explicit Tile(const Position& position) :
		position_(position) { }

	const Position& getPosition() const { return position_; }
	void setPosition(const Position& position) { position_ = position; }

	bool empty() const { return !ground_ && items_.empty() && house_id_ == 0 && flags_ == 0; }

	bool hasGround() const { return static_cast<bool>(ground_); }
	const Item* getGround() const { return ground_ ? &*ground_ : nullptr; }
	Item* getGround() { return ground_ ? &*ground_ : nullptr; }
	void setGround(Item item) { ground_ = std::move(item); }
	void clearGround() { ground_.reset(); }

	const std::vector<Item>& getItems() const { return items_; }
	std::vector<Item>& getItems() { return items_; }
	void addItem(Item item) { items_.push_back(std::move(item)); }
	void clearItems() { items_.clear(); }
	bool popTopItem() {
		if (items_.empty()) {
			return false;
		}
		items_.pop_back();
		return true;
	}

	uint32_t getFlags() const { return flags_; }
	void setFlags(uint32_t flags) { flags_ = flags; }
	bool hasFlag(uint32_t flag) const { return (flags_ & flag) != 0; }

	uint32_t getHouseID() const { return house_id_; }
	void setHouseID(uint32_t id) { house_id_ = id; }
	void setFlag(uint32_t flag) { flags_ |= flag; }
	void clearFlag(uint32_t flag) { flags_ &= ~flag; }

	std::size_t itemCount() const { return (ground_ ? 1 : 0) + items_.size(); }

	Tile deepCopy() const { return *this; }

private:
	Position position_;
	std::optional<Item> ground_;
	std::vector<Item> items_;
	uint32_t flags_ = 0;
	uint32_t house_id_ = 0;
};

} // namespace core
} // namespace rme
