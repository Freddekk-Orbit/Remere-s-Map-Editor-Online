#pragma once

// Undo/redo queue modeled on upstream source/action.h (tile swap changes).

#include "map.h"

#include <memory>
#include <string>
#include <vector>

namespace rme {
namespace core {

enum class ActionIdentifier {
	Draw,
	Erase,
	Replace,
	Load,
	NewMap,
	BrushStroke,
	Fill,
	Paste,
	Transform,
};

struct TileChange {
	Position position;
	Tile before;
	Tile after;
};

class Action {
public:
	explicit Action(ActionIdentifier type) :
		type_(type) { }

	void addChange(TileChange change) { changes_.push_back(std::move(change)); }
	std::vector<TileChange>& changes() { return changes_; }
	const std::vector<TileChange>& changes() const { return changes_; }
	ActionIdentifier type() const { return type_; }

	void commit(Map& map);
	void undo(Map& map);

private:
	ActionIdentifier type_;
	std::vector<TileChange> changes_;
};

class ActionQueue {
public:
	explicit ActionQueue(std::size_t limit = 256) :
		limit_(limit) { }

	void add(Action action, Map& map);
	void record(Action action);
	bool canUndo() const { return index_ > 0; }
	bool canRedo() const { return index_ < actions_.size(); }
	bool undo(Map& map);
	bool redo(Map& map);
	void clear();
	std::size_t undoDepth() const { return index_; }
	std::size_t size() const { return actions_.size(); }

private:
	std::vector<Action> actions_;
	std::size_t index_ = 0;
	std::size_t limit_;
};

} // namespace core
} // namespace rme
