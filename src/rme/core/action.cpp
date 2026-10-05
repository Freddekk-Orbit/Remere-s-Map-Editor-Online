#include "action.h"

namespace rme {
namespace core {

void Action::commit(Map& map) {
	for (const TileChange& change : changes_) {
		if (change.after.empty()) {
			map.removeTile(change.position);
		} else {
			map.setTile(change.after.deepCopy());
		}
	}
}

void Action::undo(Map& map) {
	for (auto it = changes_.rbegin(); it != changes_.rend(); ++it) {
		if (it->before.empty()) {
			map.removeTile(it->position);
		} else {
			map.setTile(it->before.deepCopy());
		}
	}
}

void ActionQueue::add(Action action, Map& map) {
	if (index_ < actions_.size()) {
		actions_.erase(actions_.begin() + static_cast<std::ptrdiff_t>(index_), actions_.end());
	}
	action.commit(map);
	actions_.push_back(std::move(action));
	if (actions_.size() > limit_) {
		actions_.erase(actions_.begin());
	}
	index_ = actions_.size();
}

bool ActionQueue::undo(Map& map) {
	if (!canUndo()) {
		return false;
	}
	actions_[index_ - 1].undo(map);
	--index_;
	return true;
}

bool ActionQueue::redo(Map& map) {
	if (!canRedo()) {
		return false;
	}
	actions_[index_].commit(map);
	++index_;
	return true;
}

void ActionQueue::clear() {
	actions_.clear();
	index_ = 0;
}

} // namespace core
} // namespace rme
