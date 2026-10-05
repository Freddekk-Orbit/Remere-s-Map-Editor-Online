#include "platform/platform.h"
#include "rme/core/session.h"
#include "wasm/asset_bridge.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include <SDL.h>

#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
	#include <emscripten/html5.h>
	#include <SDL_opengles2.h>
#else
	#include <SDL_opengl.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace {

const char* AssetKindLabel(rme::wasm::AssetKind kind) {
	switch (kind) {
		case rme::wasm::AssetKind::Otbm:
			return ".otbm map";
		case rme::wasm::AssetKind::Dat:
			return ".dat items";
		case rme::wasm::AssetKind::Spr:
			return ".spr sprites";
		case rme::wasm::AssetKind::Xml:
			return "XML";
		case rme::wasm::AssetKind::Otgz:
			return ".otgz archive";
		case rme::wasm::AssetKind::Other:
			return "file";
		case rme::wasm::AssetKind::Unknown:
		default:
			return "unknown";
	}
}

class WasmEditorApp {
public:
	bool Init() {
#ifdef __EMSCRIPTEN__
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
#else
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
#endif
			std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
			return false;
		}

#ifdef __EMSCRIPTEN__
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		glsl_version_ = "#version 300 es";
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		glsl_version_ = "#version 130";
#endif

#ifdef SDL_HINT_IME_SHOW_UI
		SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		window_ = SDL_CreateWindow(
			"Remere's Map Editor (Wasm)",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			1280,
			800,
			static_cast<SDL_WindowFlags>(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI)
		);
		if (!window_) {
			std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
			return false;
		}

		gl_context_ = SDL_GL_CreateContext(window_);
		if (!gl_context_) {
			std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
			return false;
		}

		SDL_GL_MakeCurrent(window_, gl_context_);
		SDL_GL_SetSwapInterval(1);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef __EMSCRIPTEN__
		// MEMFS has no useful home path for imgui.ini; persist later via IDBFS if needed.
		io.IniFilename = nullptr;
#endif

		ImGui::StyleColorsDark();
		ApplyEditorStyle();

		ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
		ImGui_ImplOpenGL3_Init(glsl_version_);

		rme::wasm::InitVirtualFileSystem();
		session_.createSampleMap(std::string(rme::wasm::kUploadDir) + "/sample.otbm");
		return true;
	}

	void SyncCanvasToCssSize() {
#ifdef __EMSCRIPTEN__
		double css_w = 0.0;
		double css_h = 0.0;
		emscripten_get_element_css_size("#canvas", &css_w, &css_h);
		const int width = std::max(1, static_cast<int>(css_w));
		const int height = std::max(1, static_cast<int>(css_h));
		int fb_w = 0;
		int fb_h = 0;
		emscripten_get_canvas_element_size("#canvas", &fb_w, &fb_h);
		if (fb_w != width || fb_h != height) {
			emscripten_set_canvas_element_size("#canvas", width, height);
			SDL_SetWindowSize(window_, width, height);
		}
#endif
	}

	bool Frame() {
		SyncCanvasToCssSize();
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT) {
				running_ = false;
			}
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE
				&& event.window.windowID == SDL_GetWindowID(window_)) {
				running_ = false;
			}
		}

#ifdef __EMSCRIPTEN__
		// Browser tab close is the real exit; keep the loop alive.
		running_ = true;
#endif

		if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) {
			SDL_Delay(10);
			return running_;
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		DrawUi();

		ImGui::Render();
		const ImGuiIO& io = ImGui::GetIO();
		glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
		glClearColor(clear_color_.x * clear_color_.w, clear_color_.y * clear_color_.w, clear_color_.z * clear_color_.w, clear_color_.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window_);
		return running_;
	}

	void Shutdown() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		if (gl_context_) {
			SDL_GL_DeleteContext(gl_context_);
			gl_context_ = nullptr;
		}
		if (window_) {
			SDL_DestroyWindow(window_);
			window_ = nullptr;
		}
		SDL_Quit();
	}

private:
	void ApplyEditorStyle() {
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 4.0f;
		style.FrameRounding = 3.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 3.0f;
		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.ScrollbarSize = 16.0f;
		ImGui::GetIO().FontGlobalScale = 1.15f;
		style.Colors[ImGuiCol_Text] = ImVec4(0.94f, 0.95f, 0.96f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.62f, 0.66f, 0.68f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.17f, 0.19f, 0.98f);
		style.Colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.15f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.16f, 0.18f, 0.98f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.18f, 0.20f, 0.22f, 1.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.18f, 0.24f, 0.20f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.22f, 0.40f, 0.28f, 1.00f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.24f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.22f, 0.40f, 0.28f, 0.90f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.50f, 0.34f, 0.95f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.24f, 0.46f, 0.30f, 0.95f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.56f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.20f, 0.28f, 0.22f, 1.00f);
		style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.17f, 0.18f, 0.20f, 1.00f);
		style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.19f, 0.21f, 0.22f, 1.00f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.38f, 0.42f, 0.40f, 0.70f);
	}

	void DrawUi() {
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New map")) {
					session_.newMap();
				}
				if (ImGui::MenuItem("Create sample OTBM")) {
					session_.createSampleMap(std::string(rme::wasm::kUploadDir) + "/sample.otbm");
				}
				if (ImGui::MenuItem("Upload client / map files...")) {
					rme::wasm::OpenBrowserFilePicker();
				}
				if (ImGui::MenuItem("Save map to /persist/edited.otbm")) {
					session_.saveOtbm(std::string(rme::wasm::kPersistDir) + "/edited.otbm");
					rme::wasm::SyncPersistentStore();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Undo", "Ctrl+Z", false, session_.canUndo())) {
					session_.undo();
				}
				if (ImGui::MenuItem("Redo", "Ctrl+Y", false, session_.canRedo())) {
					session_.redo();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View")) {
				ImGui::MenuItem("Map canvas", nullptr, &show_canvas_);
				ImGui::MenuItem("Map inspector", nullptr, &show_inspector_);
				ImGui::MenuItem("Asset browser", nullptr, &show_assets_);
				ImGui::MenuItem("Build log", nullptr, &show_log_);
				ImGui::MenuItem("ImGui demo", nullptr, &show_demo_);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help")) {
				ImGui::MenuItem("About Phase 1", nullptr, &show_about_);
				ImGui::EndMenu();
			}
			ImGui::SameLine(ImGui::GetWindowWidth() - 220.0f);
			ImGui::TextDisabled("WebGL2  |  %.0f FPS", ImGui::GetIO().Framerate);
			ImGui::EndMainMenuBar();
		}

		const ImGuiIO& io = ImGui::GetIO();
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
			session_.undo();
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
			session_.redo();
		}

		if (show_about_) {
			DrawAbout();
		}
		if (show_inspector_) {
			DrawMapInspector();
		}
		if (show_canvas_) {
			DrawMapCanvas();
		}
		if (show_assets_) {
			DrawAssetBrowser();
		}
		if (show_log_) {
			DrawLog();
		}
		if (show_demo_) {
			ImGui::ShowDemoWindow(&show_demo_);
		}

		DrawWelcomeOverlay();
	}

	void DrawWelcomeOverlay() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 16.0f, viewport->WorkPos.y + 28.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(460.0f, 240.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("RME Wasm — Phase 2", nullptr, ImGuiWindowFlags_NoCollapse)) {
			ImGui::End();
			return;
		}

		ImGui::TextWrapped(
			"C++ map core is live: OTBM load/save (upstream FileHandle node stream), "
			"tile edits, and undo/redo. Sprites come later; the canvas draws item IDs."
		);
		ImGui::Separator();
		ImGui::Text("Map: %s  %dx%d  tiles=%zu  items=%zu",
			session_.map().getName().c_str(),
			session_.map().getWidth(),
			session_.map().getHeight(),
			session_.map().tileCount(),
			session_.map().itemCount());
		if (!session_.lastError().empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", session_.lastError().c_str());
		}
		ImGui::Spacing();
		if (ImGui::Button("Open file picker")) {
			rme::wasm::OpenBrowserFilePicker();
		}
		ImGui::SameLine();
		if (ImGui::Button("Undo") && session_.canUndo()) {
			session_.undo();
		}
		ImGui::SameLine();
		if (ImGui::Button("Redo") && session_.canRedo()) {
			session_.redo();
		}
		ImGui::End();
	}

	void DrawAbout() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 500.0f, viewport->WorkPos.y + 28.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(520.0f, 280.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("About Remere's Map Editor (Wasm)", &show_about_)) {
			ImGui::End();
			return;
		}
		ImGui::TextUnformatted("Remere's Map Editor — WebAssembly port");
		ImGui::Separator();
		ImGui::TextWrapped(
			"Phase 2 imports the C++ map core: OTBM node I/O from Remere's FileHandle, "
			"a tile map, and an action queue for undo/redo. The browser UI stays ImGui."
		);
		ImGui::Spacing();
		ImGui::BulletText("UI: Dear ImGui (SDL2 + OpenGL ES 3.0 / WebGL2)");
		ImGui::BulletText("Core: OTBM + tiles + undo/redo in src/rme/core");
		ImGui::BulletText("wxWidgets: stubbed via src/wx_stub include path");
		ImGui::End();
	}

	void DrawAssetBrowser() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 16.0f, viewport->WorkPos.y + 280.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(720.0f, 300.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Asset browser", &show_assets_)) {
			ImGui::End();
			return;
		}

		ImGui::InputText("Fetch URL", fetch_url_, sizeof(fetch_url_));
		ImGui::SameLine();
		if (ImGui::Button("Download into /assets")) {
			StartSampleFetch();
		}

		ImGui::Separator();
		if (ImGui::BeginTable("vfs", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 110.0f);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("Path");
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableHeadersRow();

			const auto files = rme::wasm::ListVirtualFiles();
			for (const auto& file : files) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(file.name.c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(AssetKindLabel(file.kind));
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%zu", file.size);
				ImGui::TableSetColumnIndex(3);
				ImGui::TextUnformatted(file.vfs_path.c_str());
				ImGui::TableSetColumnIndex(4);
				ImGui::PushID(file.vfs_path.c_str());
				if (ImGui::SmallButton("Load")) {
					LoadVirtualFile(file);
				}
				ImGui::SameLine();
				if (!file.persistent && ImGui::SmallButton("Persist")) {
					rme::wasm::PersistUploadedFile(file.vfs_path);
				} else if (file.persistent) {
					ImGui::TextDisabled("IDBFS");
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		ImGui::End();
	}

	void DrawLog() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 750.0f, viewport->WorkPos.y + 280.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420.0f, 300.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("VFS log", &show_log_)) {
			ImGui::End();
			return;
		}
		const auto lines = rme::wasm::ConsumeLog();
		for (const auto& line : lines) {
			ImGui::TextUnformatted(line.c_str());
		}
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
			ImGui::SetScrollHereY(1.0f);
		}
		ImGui::End();
	}

	void LoadVirtualFile(const rme::wasm::VirtualFile& file) {
		switch (file.kind) {
			case rme::wasm::AssetKind::Otbm:
				session_.loadOtbm(file.vfs_path);
				break;
			case rme::wasm::AssetKind::Dat:
				session_.loadDat(file.vfs_path);
				break;
			case rme::wasm::AssetKind::Spr:
				session_.loadSpr(file.vfs_path);
				break;
			default:
				break;
		}
	}

	void DrawMapInspector() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 490.0f, viewport->WorkPos.y + 28.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(360.0f, 280.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Map inspector", &show_inspector_)) {
			ImGui::End();
			return;
		}

		const auto& map = session_.map();
		ImGui::Text("Name: %s", map.getName().c_str());
		ImGui::TextWrapped("%s", map.getDescription().c_str());
		ImGui::Text("Size: %d x %d", map.getWidth(), map.getHeight());
		ImGui::Text("OTBM v%u  items %u.%u", map.getVersion().otbm, map.getVersion().items_major, map.getVersion().items_minor);
		ImGui::Text("Tiles: %zu   Items: %zu   Floor %d: %zu",
			map.tileCount(), map.itemCount(), session_.floor(), map.countTilesOnFloor(session_.floor()));
		ImGui::Text("Towns: %zu   Waypoints: %zu", map.towns().size(), map.waypoints().size());
		ImGui::Text("Undo stack: %zu / %zu", session_.history().undoDepth(), session_.history().size());
		const auto& assets = session_.assets();
		ImGui::Separator();
		ImGui::Text(".dat: %s  items=%u outfits=%u", assets.dat_loaded ? "loaded" : "—", assets.item_count, assets.outfit_count);
		ImGui::Text(".spr: %s  sprites=%u", assets.spr_loaded ? "loaded" : "—", assets.sprite_count);
		if (!map.getWarnings().empty()) {
			ImGui::Separator();
			ImGui::Text("Warnings: %zu", map.getWarnings().size());
		}
		ImGui::End();
	}

	void DrawMapCanvas() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 16.0f, viewport->WorkPos.y + 280.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(720.0f, 520.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Map canvas", &show_canvas_)) {
			ImGui::End();
			return;
		}

		int camera_x = session_.cameraX();
		int camera_y = session_.cameraY();
		int floor = session_.floor();
		int brush = session_.brushId();
		if (ImGui::SliderInt("X", &camera_x, 0, std::max(0, session_.map().getWidth() - 1))) {
			session_.setCamera(camera_x, camera_y);
		}
		if (ImGui::SliderInt("Y", &camera_y, 0, std::max(0, session_.map().getHeight() - 1))) {
			session_.setCamera(camera_x, camera_y);
		}
		if (ImGui::SliderInt("Floor", &floor, rme::MapMinLayer, rme::MapMaxLayer)) {
			session_.setFloor(floor);
		}
		if (ImGui::InputInt("Brush item id", &brush)) {
			session_.setBrushId(static_cast<uint16_t>(std::clamp(brush, 0, 65535)));
		}
		ImGui::TextDisabled("Left-click paints ground. Shift+click erases the tile.");

		const int view = 24;
		const float tile_px = 18.0f;
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImDrawList* draw = ImGui::GetWindowDrawList();
		ImGui::InvisibleButton("map_grid", ImVec2(view * tile_px, view * tile_px));
		const bool hovered = ImGui::IsItemHovered();
		const ImVec2 mouse = ImGui::GetIO().MousePos;

		for (int row = 0; row < view; ++row) {
			for (int col = 0; col < view; ++col) {
				const int x = session_.cameraX() - view / 2 + col;
				const int y = session_.cameraY() - view / 2 + row;
				const ImVec2 p0(origin.x + col * tile_px, origin.y + row * tile_px);
				const ImVec2 p1(p0.x + tile_px - 1.0f, p0.y + tile_px - 1.0f);
				ImU32 color = IM_COL32(28, 32, 36, 255);
				const rme::core::Tile* tile = session_.map().getTile(Position(x, y, session_.floor()));
				if (tile && tile->getGround()) {
					const uint16_t id = tile->getGround()->getID();
					color = IM_COL32(40 + (id * 37) % 140, 70 + (id * 17) % 120, 50 + (id * 53) % 130, 255);
				} else if (tile && !tile->getItems().empty()) {
					color = IM_COL32(90, 70, 40, 255);
				}
				draw->AddRectFilled(p0, p1, color);
				if (x == session_.cameraX() && y == session_.cameraY()) {
					draw->AddRect(p0, p1, IM_COL32(240, 240, 240, 220));
				}
			}
		}

		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			const int col = static_cast<int>((mouse.x - origin.x) / tile_px);
			const int row = static_cast<int>((mouse.y - origin.y) / tile_px);
			if (col >= 0 && col < view && row >= 0 && row < view) {
				const Position pos(
					session_.cameraX() - view / 2 + col,
					session_.cameraY() - view / 2 + row,
					session_.floor()
				);
				if (ImGui::GetIO().KeyShift) {
					session_.eraseTile(pos);
				} else {
					session_.paintGround(pos, session_.brushId());
				}
			}
		}
		ImGui::End();
	}

	void StartSampleFetch() {
		if (fetch_url_[0] == '\0') {
			return;
		}
		std::string dest = std::string(rme::wasm::kAssetDir) + "/";
		const char* slash = std::strrchr(fetch_url_, '/');
		dest += (slash && slash[1] != '\0') ? (slash + 1) : "downloaded.bin";
		rme::wasm::FetchAsset(fetch_url_, dest, [](bool ok, const std::string& path, const std::string& error) {
			(void)ok;
			(void)path;
			(void)error;
		});
	}

	SDL_Window* window_ = nullptr;
	SDL_GLContext gl_context_ = nullptr;
	const char* glsl_version_ = nullptr;
	bool running_ = true;
	bool show_demo_ = false;
	bool show_assets_ = true;
	bool show_log_ = false;
	bool show_about_ = false;
	bool show_canvas_ = true;
	bool show_inspector_ = true;
	ImVec4 clear_color_ = ImVec4(0.07f, 0.08f, 0.09f, 1.00f);
	char fetch_url_[512] = "";
	rme::core::EditorSession session_;
};

WasmEditorApp* g_app = nullptr;

#ifdef __EMSCRIPTEN__
void EmscriptenMainLoop() {
	if (g_app) {
		g_app->Frame();
	}
}
#endif

} // namespace

int main(int, char**) {
	auto app = std::make_unique<WasmEditorApp>();
	if (!app->Init()) {
		return 1;
	}

	g_app = app.get();
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(EmscriptenMainLoop, 0, true);
	g_app = nullptr;
	return 0;
#else
	while (app->Frame()) {
	}
	g_app = nullptr;
	app->Shutdown();
	return 0;
#endif
}
