#include "session.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using rme::core::EditorSession;
using rme::core::LoadOTBM;
using rme::core::Map;

int main() {
	const auto dir = std::filesystem::temp_directory_path();
	const auto sample = (dir / "rme_phase2_sample.otbm").string();
	const auto saved = (dir / "rme_phase2_roundtrip.otbm").string();

	EditorSession session;
	if (!session.createSampleMap(sample)) {
		std::fprintf(stderr, "createSampleMap failed: %s\n", session.lastError().c_str());
		return 1;
	}
	if (session.map().tileCount() != 25) {
		std::fprintf(stderr, "expected 25 sample tiles, got %zu\n", session.map().tileCount());
		return 1;
	}

	const Position paint_pos(101, 101, session.floor());
	session.paintGround(paint_pos, 351);
	if (!session.canUndo()) {
		std::fprintf(stderr, "paint should create an undo step\n");
		return 1;
	}
	session.undo();
	if (const auto* tile = session.map().getTile(paint_pos); tile && tile->getGround() && tile->getGround()->getID() == 351) {
		std::fprintf(stderr, "undo did not restore the painted tile\n");
		return 1;
	}
	session.redo();
	const auto* painted = session.map().getTile(paint_pos);
	if (!painted || !painted->getGround() || painted->getGround()->getID() != 351) {
		std::fprintf(stderr, "redo did not reapply the painted tile\n");
		return 1;
	}

	if (!session.saveOtbm(saved)) {
		std::fprintf(stderr, "saveOtbm failed: %s\n", session.lastError().c_str());
		return 1;
	}

	EditorSession reloaded;
	if (!reloaded.loadOtbm(saved)) {
		std::fprintf(stderr, "reload failed: %s\n", reloaded.lastError().c_str());
		return 1;
	}
	if (reloaded.map().tileCount() != session.map().tileCount()) {
		std::fprintf(stderr, "roundtrip tile count mismatch %zu vs %zu\n", reloaded.map().tileCount(), session.map().tileCount());
		return 1;
	}
	const auto* again = reloaded.map().getTile(paint_pos);
	if (!again || !again->getGround() || again->getGround()->getID() != 351) {
		std::fprintf(stderr, "roundtrip lost painted item 351\n");
		return 1;
	}
	if (reloaded.map().towns().size() != 1 || reloaded.map().waypoints().size() != 1) {
		std::fprintf(stderr, "roundtrip lost town/waypoint metadata\n");
		return 1;
	}

	std::printf("rme core test ok: %zu tiles, item 351 persisted, undo/redo works\n", reloaded.map().tileCount());
	return 0;
}
