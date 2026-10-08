#include "platform/platform.h"
#include "rme/core/session.h"
#include "rme/gfx/sprite_atlas.h"
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
#include <vector>

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
		session_.createSampleAssets(
			std::string(rme::wasm::kAssetDir) + "/Tibia.dat",
			std::string(rme::wasm::kAssetDir) + "/Tibia.spr"
		);
		session_.createSampleMap(std::string(rme::wasm::kUploadDir) + "/sample.otbm");
		ReloadAtlas();
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
		atlas_.destroy();
		if (minimap_tex_ != 0) {
			glDeleteTextures(1, &minimap_tex_);
			minimap_tex_ = 0;
		}
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
				if (ImGui::MenuItem("New map...", "Ctrl+N")) {
					OpenNewMapDialog();
				}
				if (ImGui::MenuItem("Map properties...")) {
					OpenMapProperties();
				}
				if (ImGui::MenuItem("Create sample OTBM")) {
					session_.createSampleMap(std::string(rme::wasm::kUploadDir) + "/sample.otbm");
				}
				if (ImGui::MenuItem("Create sample .dat / .spr")) {
					session_.createSampleAssets(
						std::string(rme::wasm::kAssetDir) + "/Tibia.dat",
						std::string(rme::wasm::kAssetDir) + "/Tibia.spr"
					);
					ReloadAtlas();
				}
				if (ImGui::MenuItem("Upload client / map files...")) {
					rme::wasm::OpenBrowserFilePicker();
				}
				if (ImGui::MenuItem("Save to /persist", nullptr, false, true)) {
					SaveToPersist();
				}
				if (ImGui::MenuItem("Download map", "Ctrl+S")) {
					DownloadMapBundle();
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
			if (ImGui::BeginMenu("Edit")) {
				if (ImGui::MenuItem("Delete selection", "Del", false, session_.selection().visible())) {
					session_.deleteSelection();
				}
				if (ImGui::MenuItem("Copy", "Ctrl+C", false, session_.selection().visible())) {
					session_.copySelection();
				}
				if (ImGui::MenuItem("Cut", "Ctrl+X", false, session_.selection().visible())) {
					session_.cutSelection();
				}
				if (ImGui::MenuItem("Paste at camera", "Ctrl+V", false, session_.hasClipboard())) {
					session_.pasteAt(Position(session_.cameraX(), session_.cameraY(), session_.floor()));
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View")) {
				ImGui::MenuItem("Map canvas", nullptr, &show_canvas_);
				ImGui::MenuItem("Minimap", nullptr, &show_minimap_);
				ImGui::MenuItem("Towns / waypoints", nullptr, &show_markers_);
				ImGui::MenuItem("Houses", nullptr, &show_houses_);
				ImGui::MenuItem("Spawns", nullptr, &show_spawns_);
				ImGui::MenuItem("Go to position", "Ctrl+G", &show_goto_);
				ImGui::MenuItem("Tile properties", "Ctrl+I", &show_properties_);
				ImGui::MenuItem("Find items", "Ctrl+F", &show_find_);
				ImGui::MenuItem("Map issues", "Ctrl+E", &show_issues_);
				ImGui::MenuItem("Floor below", nullptr, &show_floor_below_);
				ImGui::MenuItem("Brushes", nullptr, &show_brushes_);
				ImGui::MenuItem("Item palette", nullptr, &show_palette_);
				ImGui::MenuItem("Map inspector", nullptr, &show_inspector_);
				ImGui::MenuItem("Asset browser", nullptr, &show_assets_);
				ImGui::MenuItem("Build log", nullptr, &show_log_);
				ImGui::MenuItem("ImGui demo", nullptr, &show_demo_);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help")) {
				ImGui::MenuItem("About Phase 14", nullptr, &show_about_);
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
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
			session_.copySelection();
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X, false)) {
			session_.cutSelection();
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
			session_.pasteAt(Position(session_.cameraX(), session_.cameraY(), session_.floor()));
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
			show_goto_ = true;
			goto_x_ = session_.cameraX();
			goto_y_ = session_.cameraY();
			goto_z_ = session_.floor();
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I, false)) {
			show_properties_ = true;
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
			show_find_ = true;
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E, false)) {
			show_issues_ = true;
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
			OpenNewMapDialog();
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			DownloadMapBundle();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
			session_.deleteSelection();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false)) {
			session_.setBrushSize(session_.brushSize() - 2);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) {
			session_.setBrushSize(session_.brushSize() + 2);
		}
		if (!io.WantTextInput) {
			if (ImGui::IsKeyPressed(ImGuiKey_W, true) || ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
				session_.panBy(0, -1);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_S, true) && !io.KeyCtrl) {
				session_.panBy(0, 1);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
				session_.panBy(0, 1);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_A, true) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) {
				session_.panBy(-1, 0);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_D, true) || ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) {
				session_.panBy(1, 0);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_PageUp, false)) {
				session_.setFloor(session_.floor() - 1);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_PageDown, false)) {
				session_.setFloor(session_.floor() + 1);
			}
		}

		if (show_about_) {
			DrawAbout();
		}
		if (show_inspector_) {
			DrawMapInspector();
		}
		if (show_minimap_) {
			DrawMinimap();
		}
		if (show_markers_) {
			DrawMarkers();
		}
		if (show_houses_) {
			DrawHouses();
		}
		if (show_spawns_) {
			DrawSpawns();
		}
		if (show_goto_) {
			DrawGoto();
		}
		if (show_new_map_) {
			DrawNewMap();
		}
		if (show_map_props_) {
			DrawMapProperties();
		}
		if (show_properties_) {
			DrawTileProperties();
		}
		if (show_find_) {
			DrawFind();
		}
		if (show_issues_) {
			DrawMapIssues();
		}
		if (show_brushes_) {
			DrawBrushTools();
		}
		if (show_canvas_) {
			DrawMapCanvas();
		}
		if (show_palette_) {
			DrawItemPalette();
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
		if (!ImGui::Begin("RME Wasm - Phase 14", nullptr, ImGuiWindowFlags_NoCollapse)) {
			ImGui::End();
			return;
		}

		ImGui::TextWrapped(
			"Creature brush paints monsters into spawn.xml. One drag shares a spawn when tiles stay "
			"inside its radius; otherwise a new spawn is created. Shift+click removes the creature "
			"(and an empty spawn). Red dots are monsters, yellow circle is the radius."
		);
		ImGui::Separator();
		ImGui::Text("Map: %s  %dx%d  tiles=%zu  items=%zu%s",
			session_.map().getName().c_str(),
			session_.map().getWidth(),
			session_.map().getHeight(),
			session_.map().tileCount(),
			session_.map().itemCount(),
			session_.map().hasChanged() ? "  *" : "");
		if (!session_.lastError().empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", session_.lastError().c_str());
		}
		if (!last_save_message_.empty()) {
			ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.60f, 1.0f), "%s", last_save_message_.c_str());
		}
		ImGui::Spacing();
		if (ImGui::Button("Download map")) {
			DownloadMapBundle();
		}
		ImGui::SameLine();
		if (ImGui::Button("Map properties")) {
			OpenMapProperties();
		}
		ImGui::SameLine();
		if (ImGui::Button("New map")) {
			OpenNewMapDialog();
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
		ImGui::TextUnformatted("Remere's Map Editor - WebAssembly port");
		ImGui::Separator();
		ImGui::TextWrapped(
			"Phase 14 adds a Creature brush. Painting writes spawn.xml monsters, not OTBM items. "
			"Pick a name and radius, then click tiles. Shift+click clears that tile's creature. "
			"Spawn list edits still skip the tile undo stack."
		);
		ImGui::Spacing();
		ImGui::BulletText("UI: Dear ImGui (SDL2 + OpenGL ES 3.0 / WebGL2)");
		ImGui::BulletText("Core: OTBM + DAT/SPR + brushes + minimap in src/rme/core");
		ImGui::BulletText("GPU atlas: src/rme/gfx (GL stays out of the map core)");
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
				ReloadAtlas();
				break;
			case rme::wasm::AssetKind::Xml: {
				const auto& name = file.name;
				if (name.find("spawn") != std::string::npos) {
					session_.loadSpawnXml(file.vfs_path);
				} else if (name.find("material") != std::string::npos) {
					session_.loadMaterials(file.vfs_path);
				} else {
					session_.loadHouseXml(file.vfs_path);
				}
				break;
			}
			default:
				break;
		}
	}

	void ReloadAtlas() {
		atlas_.upload(session_.sprites());
	}

	ImTextureID AtlasTexture() const {
		return static_cast<ImTextureID>(static_cast<intptr_t>(atlas_.texture()));
	}

	void DrawSprite(ImDrawList* draw, const ImVec2& p0, const ImVec2& p1, uint16_t sprite_id, ImU32 fallback, ImU32 tint = IM_COL32_WHITE) const {
		if (atlas_.valid() && atlas_.has(sprite_id)) {
			const auto uv = atlas_.uv(sprite_id);
			draw->AddImage(AtlasTexture(), p0, p1, ImVec2(uv.u0, uv.v0), ImVec2(uv.u1, uv.v1), tint);
			return;
		}
		const ImU32 alpha = (tint >> IM_COL32_A_SHIFT) & 0xFF;
		draw->AddRectFilled(p0, p1, (fallback & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT));
	}

	static ImU32 FallbackColor(uint16_t id) {
		return IM_COL32(40 + (id * 37) % 140, 70 + (id * 17) % 120, 50 + (id * 53) % 130, 255);
	}

	void DrawMapInspector() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 490.0f, viewport->WorkPos.y + 28.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(400.0f, 300.0f), ImGuiCond_FirstUseEver);
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
		ImGui::Text("Towns: %zu   Waypoints: %zu   Houses: %zu   Spawns: %zu",
			map.towns().size(), map.waypoints().size(), map.houses().size(), map.spawns().size());
		ImGui::Text("Undo stack: %zu / %zu", session_.history().undoDepth(), session_.history().size());
		const auto& assets = session_.assets();
		ImGui::Separator();
		ImGui::Text(".dat: %s  items=%zu (header %u)", assets.dat_loaded ? "loaded" : "-", session_.items().size(), assets.item_count);
		ImGui::Text(".spr: %s  sprites=%u  atlas=%s",
			assets.spr_loaded ? "loaded" : "-",
			assets.sprite_count,
			atlas_.valid() ? "ready" : "-");
		if (const auto* brush = session_.brushType()) {
			ImGui::Text("Brush: %u %s  sprite=%u", brush->id, brush->name.c_str(), brush->sprite_id);
		}
		ImGui::Text("Tool: %s -> %s  size %d  sel %zu  clip %zu",
			rme::core::BrushKindName(session_.brushKind()),
			rme::core::BrushKindName(session_.resolvedBrush()),
			session_.brushSize(),
			session_.selection().size(),
			session_.clipboardSize());
		ImGui::Text("Camera %d,%d  floor %d  zoom %.2f", session_.cameraX(), session_.cameraY(), session_.floor(), zoom_);
		ImGui::Text("House id %u", session_.houseId());
		ImGui::Text("Inspect %d,%d,%d", session_.inspect().x, session_.inspect().y, session_.inspect().z);
		if (!map.getWarnings().empty()) {
			ImGui::Separator();
			ImGui::Text("Warnings: %zu", map.getWarnings().size());
		}
		ImGui::End();
	}

	void SyncMinimapTexture() {
		const rme::core::Minimap& mm = session_.minimap();
		if (mm.empty() || mm.width() <= 0 || mm.height() <= 0) {
			return;
		}
		if (minimap_tex_ == 0) {
			glGenTextures(1, &minimap_tex_);
		}
		glBindTexture(GL_TEXTURE_2D, minimap_tex_);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (minimap_tex_w_ != mm.width() || minimap_tex_h_ != mm.height()) {
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RGBA,
				mm.width(),
				mm.height(),
				0,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				mm.rgba().data()
			);
			minimap_tex_w_ = mm.width();
			minimap_tex_h_ = mm.height();
		} else {
			glTexSubImage2D(
				GL_TEXTURE_2D,
				0,
				0,
				0,
				mm.width(),
				mm.height(),
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				mm.rgba().data()
			);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void DrawMinimap() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 900.0f, viewport->WorkPos.y + 28.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(280.0f, 320.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Minimap", &show_minimap_)) {
			ImGui::End();
			return;
		}

		SyncMinimapTexture();
		const rme::core::Minimap& mm = session_.minimap();
		if (minimap_tex_ == 0 || mm.empty()) {
			ImGui::TextDisabled("No minimap.");
			ImGui::End();
			return;
		}

		const int crop = std::max(8, std::min({48, mm.width(), mm.height()}));
		const int x0 = std::clamp(session_.cameraX() - crop / 2, 0, mm.width() - crop);
		const int y0 = std::clamp(session_.cameraY() - crop / 2, 0, mm.height() - crop);
		ImGui::Text("Floor %d  %dx%d  view %d,%d", mm.floor(), mm.width(), mm.height(), x0, y0);
		ImGui::TextDisabled("Click to jump. Yellow = town, cyan = waypoint.");

		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float side = std::max(64.0f, std::min(avail.x, avail.y - 8.0f));
		const ImVec2 image_size(side, side);
		const ImVec2 image_min = ImGui::GetCursorScreenPos();
		const ImTextureID tex = static_cast<ImTextureID>(static_cast<intptr_t>(minimap_tex_));
		const ImVec2 uv0(
			static_cast<float>(x0) / static_cast<float>(mm.width()),
			static_cast<float>(y0) / static_cast<float>(mm.height())
		);
		const ImVec2 uv1(
			static_cast<float>(x0 + crop) / static_cast<float>(mm.width()),
			static_cast<float>(y0 + crop) / static_cast<float>(mm.height())
		);
		ImGui::Image(tex, image_size, uv0, uv1);
		const bool hovered = ImGui::IsItemHovered();
		ImDrawList* draw = ImGui::GetWindowDrawList();

		auto mapToScreen = [&](int x, int y) -> ImVec2 {
			return ImVec2(
				image_min.x + (static_cast<float>(x - x0) + 0.5f) / static_cast<float>(crop) * image_size.x,
				image_min.y + (static_cast<float>(y - y0) + 0.5f) / static_cast<float>(crop) * image_size.y
			);
		};
		auto inCrop = [&](int x, int y) {
			return x >= x0 && y >= y0 && x < x0 + crop && y < y0 + crop;
		};

		const float tile_w = image_size.x / static_cast<float>(crop);
		const float tile_h = image_size.y / static_cast<float>(crop);
		const ImVec2 cam = mapToScreen(session_.cameraX(), session_.cameraY());
		draw->AddRect(
			ImVec2(cam.x - 9.0f * tile_w, cam.y - 7.0f * tile_h),
			ImVec2(cam.x + 9.0f * tile_w, cam.y + 7.0f * tile_h),
			IM_COL32(255, 255, 255, 220)
		);
		draw->AddCircleFilled(cam, 3.0f, IM_COL32(255, 255, 255, 255));

		for (const auto& town : session_.map().towns()) {
			if (town.temple.z != session_.floor() || !inCrop(town.temple.x, town.temple.y)) {
				continue;
			}
			const ImVec2 p = mapToScreen(town.temple.x, town.temple.y);
			draw->AddCircleFilled(p, 5.0f, IM_COL32(250, 210, 40, 255));
			draw->AddCircle(p, 5.0f, IM_COL32(20, 20, 20, 220));
		}
		for (const auto& waypoint : session_.map().waypoints()) {
			if (waypoint.position.z != session_.floor() || !inCrop(waypoint.position.x, waypoint.position.y)) {
				continue;
			}
			const ImVec2 p = mapToScreen(waypoint.position.x, waypoint.position.y);
			draw->AddRectFilled(ImVec2(p.x - 3.5f, p.y - 3.5f), ImVec2(p.x + 3.5f, p.y + 3.5f), IM_COL32(40, 220, 230, 255));
		}

		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			const ImVec2 mouse = ImGui::GetIO().MousePos;
			const int mx = x0 + static_cast<int>((mouse.x - image_min.x) / image_size.x * static_cast<float>(crop));
			const int my = y0 + static_cast<int>((mouse.y - image_min.y) / image_size.y * static_cast<float>(crop));
			session_.setCamera(mx, my);
		}

		ImGui::End();
	}

	void DrawMarkers() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 900.0f, viewport->WorkPos.y + 360.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(280.0f, 280.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Towns / waypoints", &show_markers_)) {
			ImGui::End();
			return;
		}

		const Position here(session_.cameraX(), session_.cameraY(), session_.floor());
		ImGui::Text("Camera %d, %d, %d", here.x, here.y, here.z);
		ImGui::InputText("Town name", town_name_, sizeof(town_name_));
		if (ImGui::Button("Add town here")) {
			session_.addTown(town_name_, here);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add waypoint here")) {
			session_.addWaypoint(waypoint_name_, here);
		}
		ImGui::InputText("Waypoint name", waypoint_name_, sizeof(waypoint_name_));

		ImGui::Separator();
		ImGui::TextUnformatted("Towns");
		if (session_.map().towns().empty()) {
			ImGui::TextDisabled("None");
		}
		for (const auto& town : session_.map().towns()) {
			ImGui::PushID(static_cast<int>(town.id));
			ImGui::Text("%s  (%d,%d,%d)", town.name.c_str(), town.temple.x, town.temple.y, town.temple.z);
			if (ImGui::SmallButton("Go")) {
				session_.goToTown(town.id);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete")) {
				session_.removeTown(town.id);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Waypoints");
		if (session_.map().waypoints().empty()) {
			ImGui::TextDisabled("None");
		}
		for (std::size_t i = 0; i < session_.map().waypoints().size(); ++i) {
			const auto& waypoint = session_.map().waypoints()[i];
			ImGui::PushID(static_cast<int>(i) + 10000);
			ImGui::Text("%s  (%d,%d,%d)", waypoint.name.c_str(), waypoint.position.x, waypoint.position.y, waypoint.position.z);
			if (ImGui::SmallButton("Go")) {
				session_.goToWaypoint(i);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete")) {
				session_.removeWaypoint(i);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		ImGui::End();
	}

	void DrawHouses() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 610.0f, viewport->WorkPos.y + 28.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(280.0f, 260.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Houses", &show_houses_)) {
			ImGui::End();
			return;
		}

		const Position here(session_.cameraX(), session_.cameraY(), session_.floor());
		ImGui::InputText("Name", house_name_, sizeof(house_name_));
		if (ImGui::Button("Add house here")) {
			session_.addHouse(house_name_, here);
		}
		ImGui::Text("Active house id %u", session_.houseId());
		ImGui::Separator();
		if (session_.map().houses().empty()) {
			ImGui::TextDisabled("None");
		}
		for (const auto& house : session_.map().houses()) {
			ImGui::PushID(static_cast<int>(house.id));
			const bool selected = session_.houseId() == house.id;
			if (selected) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.55f, 1.0f, 1.0f));
			}
			ImGui::Text("%s  id %u  %zu tiles", house.name.c_str(), house.id, session_.map().houseTileCount(house.id));
			if (selected) {
				ImGui::PopStyleColor();
			}
			ImGui::TextDisabled("entry %d,%d,%d", house.entry.x, house.entry.y, house.entry.z);
			if (ImGui::SmallButton("Use")) {
				session_.setHouseId(house.id);
				session_.setBrushKind(rme::core::BrushKind::House);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Go")) {
				session_.goToHouse(house.id);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete")) {
				session_.removeHouse(house.id);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		ImGui::TextDisabled("House brush paints magenta tiles. Shift+click clears.");
		ImGui::End();
	}

	void DrawSpawns() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 610.0f, viewport->WorkPos.y + 300.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(280.0f, 280.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Spawns", &show_spawns_)) {
			ImGui::End();
			return;
		}

		const Position here(session_.cameraX(), session_.cameraY(), session_.floor());
		if (ImGui::SliderInt("Radius", &spawn_radius_, 1, 16)) {
			session_.setSpawnRadius(spawn_radius_);
		}
		if (ImGui::InputText("Monster", monster_name_, sizeof(monster_name_))) {
			session_.setCreatureName(monster_name_);
		}
		if (ImGui::Button("Add spawn here")) {
			session_.addSpawn(here, session_.spawnRadius(), session_.creatureName());
		}
		ImGui::Separator();
		if (session_.map().spawns().empty()) {
			ImGui::TextDisabled("None");
		}
		for (std::size_t i = 0; i < session_.map().spawns().size(); ++i) {
			const auto& spawn = session_.map().spawns()[i];
			ImGui::PushID(static_cast<int>(i) + 20000);
			ImGui::Text("(%d,%d,%d) r=%d  %zu mobs",
				spawn.center.x, spawn.center.y, spawn.center.z, spawn.radius, spawn.monsters.size());
			if (ImGui::SmallButton("Go")) {
				session_.goToSpawn(i);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Add mob")) {
				session_.addSpawnMonster(i, monster_name_);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete")) {
				session_.removeSpawn(i);
				ImGui::PopID();
				break;
			}
			for (const auto& creature : spawn.monsters) {
				ImGui::BulletText("%s %+d,%+d t=%u", creature.name.c_str(), creature.dx, creature.dy, creature.spawntime);
			}
			ImGui::PopID();
		}
		ImGui::TextDisabled("Yellow circle is the spawn radius. Red dots are monsters.");
		ImGui::TextDisabled("Creature brush paints these names onto the map.");
		ImGui::End();
	}

	void OpenNewMapDialog() {
		show_new_map_ = true;
		std::snprintf(new_map_name_, sizeof(new_map_name_), "Untitled.otbm");
		new_map_w_ = 256;
		new_map_h_ = 256;
	}

	void OpenMapProperties() {
		show_map_props_ = true;
		std::snprintf(map_name_, sizeof(map_name_), "%s", session_.map().getName().c_str());
		std::snprintf(map_desc_, sizeof(map_desc_), "%s", session_.map().getDescription().c_str());
	}

	std::string PersistOtbmPath() const {
		return std::string(rme::wasm::kPersistDir) + "/" + session_.otbmFileName();
	}

	std::string PersistZipPath() const {
		return std::string(rme::wasm::kPersistDir) + "/" + session_.zipFileName();
	}

	void SaveToPersist() {
		const auto path = PersistOtbmPath();
		if (!session_.saveOtbm(path)) {
			last_save_message_.clear();
			return;
		}
		rme::wasm::SyncPersistentStore();
		last_save_message_ = "Saved " + path;
	}

	void DownloadMapBundle() {
		const auto zip_path = PersistZipPath();
		if (!session_.saveMapZip(zip_path)) {
			last_save_message_.clear();
			return;
		}
		rme::wasm::SyncPersistentStore();
		if (rme::wasm::DownloadVfsFile(zip_path, session_.zipFileName())) {
			last_save_message_ = "Downloaded " + session_.zipFileName() + " (also in /persist)";
		} else {
			last_save_message_ = "Wrote " + zip_path + " (browser download unavailable)";
		}
	}

	void DrawNewMap() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 360.0f, viewport->WorkPos.y + 90.0f), ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(ImVec2(320.0f, 180.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("New map", &show_new_map_)) {
			ImGui::End();
			return;
		}
		ImGui::InputText("Name", new_map_name_, sizeof(new_map_name_));
		ImGui::InputInt("Width", &new_map_w_);
		ImGui::InputInt("Height", &new_map_h_);
		ImGui::TextDisabled("Size is clamped to %d-%d.", rme::MapMinWidth, rme::MapMaxWidth);
		if (ImGui::Button("Create")) {
			session_.newMap(new_map_w_, new_map_h_, new_map_name_);
			last_save_message_.clear();
			show_new_map_ = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			show_new_map_ = false;
		}
		ImGui::End();
	}

	void DrawMapProperties() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 380.0f, viewport->WorkPos.y + 110.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420.0f, 220.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Map properties", &show_map_props_)) {
			ImGui::End();
			return;
		}
		ImGui::InputText("Name", map_name_, sizeof(map_name_));
		ImGui::InputTextMultiline("Description", map_desc_, sizeof(map_desc_), ImVec2(-1.0f, 72.0f));
		ImGui::Text("Size %d x %d  tiles %zu",
			session_.map().getWidth(), session_.map().getHeight(), session_.map().tileCount());
		if (ImGui::Button("Apply")) {
			session_.setMapName(map_name_);
			session_.setMapDescription(map_desc_);
			std::snprintf(map_name_, sizeof(map_name_), "%s", session_.map().getName().c_str());
		}
		ImGui::SameLine();
		if (ImGui::Button("Revert")) {
			OpenMapProperties();
		}
		ImGui::TextDisabled("Name becomes the .otbm / .zip file name. Description is stored in the OTBM.");
		ImGui::End();
	}

	void DrawGoto() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 400.0f, viewport->WorkPos.y + 120.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(260.0f, 160.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Go to position", &show_goto_)) {
			ImGui::End();
			return;
		}
		ImGui::InputInt("X", &goto_x_);
		ImGui::InputInt("Y", &goto_y_);
		ImGui::InputInt("Z", &goto_z_);
		if (ImGui::Button("Go")) {
			session_.goTo(goto_x_, goto_y_, goto_z_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Use camera")) {
			goto_x_ = session_.cameraX();
			goto_y_ = session_.cameraY();
			goto_z_ = session_.floor();
		}
		ImGui::End();
	}

	void SyncPropertyBuffers() {
		const Position pos = session_.inspect();
		const uint64_t key = rme::core::MakeTileKey(pos.x, pos.y, pos.z);
		const rme::core::Item* item = session_.inspectItem();
		const uint16_t serial = item ? item->getID() : 0;
		const uint64_t stamp = (key << 16) ^ serial ^ (session_.history().undoDepth() << 8)
			^ (static_cast<uint64_t>(session_.inspectIndex()) << 40);
		if (stamp == props_stamp_) {
			return;
		}
		props_stamp_ = stamp;
		if (!item) {
			prop_aid_ = 0;
			prop_uid_ = 0;
			prop_count_ = 1;
			prop_charges_ = 0;
			prop_depot_ = 0;
			prop_door_ = 0;
			prop_has_dest_ = false;
			prop_dx_ = 0;
			prop_dy_ = 0;
			prop_dz_ = rme::MapGroundLayer;
			prop_text_[0] = '\0';
			prop_desc_[0] = '\0';
			return;
		}
		const auto props = rme::core::PropsFromItem(*item);
		prop_aid_ = props.action_id;
		prop_uid_ = props.unique_id;
		prop_count_ = props.count;
		prop_charges_ = props.charges;
		prop_depot_ = props.depot_id;
		prop_door_ = props.door_id;
		prop_has_dest_ = props.has_destination;
		prop_dx_ = props.destination.x;
		prop_dy_ = props.destination.y;
		prop_dz_ = props.destination.z;
		std::snprintf(prop_text_, sizeof(prop_text_), "%s", props.text.c_str());
		std::snprintf(prop_desc_, sizeof(prop_desc_), "%s", props.description.c_str());
	}

	void DrawTileProperties() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 1210.0f, viewport->WorkPos.y + 28.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(360.0f, 620.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(280.0f, 320.0f), ImVec2(FLT_MAX, FLT_MAX));
		if (!ImGui::Begin("Tile properties", &show_properties_)) {
			ImGui::End();
			return;
		}

		if (follow_camera_) {
			session_.setInspect(Position(session_.cameraX(), session_.cameraY(), session_.floor()));
		}
		SyncPropertyBuffers();

		const Position pos = session_.inspect();
		ImGui::Text("Tile %d, %d, %d", pos.x, pos.y, pos.z);
		ImGui::Checkbox("Follow camera", &follow_camera_);
		if (ImGui::Button("Inspect camera")) {
			follow_camera_ = false;
			session_.setInspect(Position(session_.cameraX(), session_.cameraY(), session_.floor()));
			props_stamp_ = 0;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("RMB inspects");

		ImGui::Separator();
		ImGui::TextUnformatted("Stack (ground first)");
		const auto stack = session_.browseInspect();
		if (ImGui::BeginChild("tile_stack", ImVec2(0.0f, 92.0f), true)) {
			for (const auto& entry : stack) {
				const auto* type = session_.items().get(entry.item_id);
				char line[160];
				std::snprintf(line, sizeof(line), "%s %u %s%s",
					entry.is_ground ? "G" : "I",
					entry.item_id,
					type ? type->name.c_str() : "",
					entry.content_count ? " [box]" : "");
				ImGui::PushID(entry.index);
				if (ImGui::Selectable(line, session_.inspectIndex() == entry.index)) {
					session_.setInspectIndex(entry.index);
					props_stamp_ = 0;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		if (ImGui::Button("Up")) {
			session_.moveInspectItem(1);
			props_stamp_ = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("Down")) {
			session_.moveInspectItem(-1);
			props_stamp_ = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove") && session_.removeInspectItem()) {
			props_stamp_ = 0;
		}

		const rme::core::Item* item = session_.inspectItem();
		if (!item) {
			ImGui::TextDisabled("No item on this tile.");
			ImGui::End();
			return;
		}
		const auto* type = session_.items().get(item->getID());
		ImGui::Text("Item %u  %s%s", item->getID(), type ? type->name.c_str() : "",
			type && type->container ? "  (container)" : "");
		ImGui::InputInt("Action ID", &prop_aid_);
		ImGui::InputInt("Unique ID", &prop_uid_);
		ImGui::InputInt("Count", &prop_count_);
		ImGui::InputInt("Charges", &prop_charges_);
		ImGui::InputInt("Depot ID", &prop_depot_);
		ImGui::InputInt("Door ID", &prop_door_);
		ImGui::InputText("Text", prop_text_, sizeof(prop_text_));
		ImGui::InputText("Description", prop_desc_, sizeof(prop_desc_));
		ImGui::Checkbox("Teleport", &prop_has_dest_);
		if (prop_has_dest_) {
			ImGui::InputInt("Dest X", &prop_dx_);
			ImGui::InputInt("Dest Y", &prop_dy_);
			ImGui::InputInt("Dest Z", &prop_dz_);
		}
		if (ImGui::Button("Apply")) {
			rme::core::ItemProps props;
			props.action_id = static_cast<uint16_t>(std::clamp(prop_aid_, 0, 65535));
			props.unique_id = static_cast<uint16_t>(std::clamp(prop_uid_, 0, 65535));
			props.count = static_cast<uint8_t>(std::clamp(prop_count_, 1, 255));
			props.charges = static_cast<uint16_t>(std::clamp(prop_charges_, 0, 65535));
			props.depot_id = static_cast<uint16_t>(std::clamp(prop_depot_, 0, 65535));
			props.door_id = static_cast<uint8_t>(std::clamp(prop_door_, 0, 255));
			props.has_destination = prop_has_dest_;
			props.destination = Position(prop_dx_, prop_dy_, prop_dz_);
			props.text = prop_text_;
			props.description = prop_desc_;
			session_.editTopItem(props);
			props_stamp_ = 0;
			item = session_.inspectItem();
		}
		if (item && item->hasDestination()) {
			ImGui::SameLine();
			if (ImGui::Button("Go to dest")) {
				follow_camera_ = false;
				session_.goTo(item->getDestination().x, item->getDestination().y, item->getDestination().z);
			}
		}
		ImGui::TextDisabled("Apply / stack edits are one undo step.");

		item = session_.inspectItem();
		type = item ? session_.items().get(item->getID()) : nullptr;
		if (item && ((type && type->container) || !item->getContents().empty())) {
			ImGui::Separator();
			ImGui::Text("Container  %zu item(s)", item->getContents().size());
			if (ImGui::BeginChild("container_list", ImVec2(0.0f, 72.0f), true)) {
				int nested = 0;
				for (const auto& inside : item->getContents()) {
					const auto* nested_type = session_.items().get(inside.getID());
					ImGui::Text("%u %s  x%u", inside.getID(), nested_type ? nested_type->name.c_str() : "",
						inside.getCount());
					ImGui::SameLine();
					ImGui::PushID(nested);
					if (ImGui::SmallButton("X")) {
						session_.removeContainerItem(static_cast<std::size_t>(nested));
						props_stamp_ = 0;
						ImGui::PopID();
						break;
					}
					ImGui::PopID();
					++nested;
				}
			}
			ImGui::EndChild();
			ImGui::InputInt("Add id", &container_add_id_);
			ImGui::SameLine();
			if (ImGui::Button("Add inside")) {
				session_.addContainerItem(static_cast<uint16_t>(std::clamp(container_add_id_, 0, 65535)));
				props_stamp_ = 0;
			}
		}
		ImGui::End();
	}

	void DrawMapIssues() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 610.0f, viewport->WorkPos.y + 520.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(360.0f, 220.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Map issues", &show_issues_)) {
			ImGui::End();
			return;
		}
		const auto issues = session_.mapIssues();
		ImGui::Text("%zu issue(s)", issues.size());
		if (ImGui::BeginChild("issue_list", ImVec2(0.0f, 0.0f), true)) {
			int index = 0;
			for (const auto& issue : issues) {
				char line[256];
				std::snprintf(line, sizeof(line), "%s  %d,%d,%d  %s",
					rme::core::MapIssueKindName(issue.kind),
					issue.position.x, issue.position.y, issue.position.z,
					issue.message.c_str());
				ImGui::PushID(index);
				if (ImGui::Selectable(line)) {
					follow_camera_ = false;
					session_.goToIssue(static_cast<std::size_t>(index));
					show_properties_ = true;
					props_stamp_ = 0;
				}
				ImGui::PopID();
				++index;
			}
		}
		ImGui::EndChild();
		ImGui::End();
	}

	void DrawFind() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 1210.0f, viewport->WorkPos.y + 500.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(340.0f, 230.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 180.0f), ImVec2(FLT_MAX, FLT_MAX));
		if (!ImGui::Begin("Find items", &show_find_)) {
			ImGui::End();
			return;
		}
		ImGui::InputInt("Item id", &find_id_);
		ImGui::InputInt("Action ID", &find_aid_);
		ImGui::InputInt("Unique ID", &find_uid_);
		ImGui::Checkbox("Teleports only", &find_teleports_);
		rme::core::FindQuery query;
		query.item_id = static_cast<uint16_t>(std::clamp(find_id_, 0, 65535));
		query.action_id = static_cast<uint16_t>(std::clamp(find_aid_, 0, 65535));
		query.unique_id = static_cast<uint16_t>(std::clamp(find_uid_, 0, 65535));
		query.teleports_only = find_teleports_;
		ImGui::Text("Matches: %zu", session_.findTiles(query).size());
		if (ImGui::Button("Find next")) {
			follow_camera_ = false;
			session_.findNext(query);
			show_properties_ = true;
			props_stamp_ = 0;
		}
		ImGui::TextDisabled("0 means ignore that field. Ctrl+F opens this window.");
		ImGui::End();
	}

	void DrawMapCanvas() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 16.0f, viewport->WorkPos.y + 280.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(580.0f, 680.0f), ImGuiCond_FirstUseEver);
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
		if (const auto* brush_type = session_.brushType()) {
			ImGui::SameLine();
			ImGui::TextDisabled("%s / %s", brush_type->name.c_str(), rme::core::BrushKindName(session_.resolvedBrush()));
		}
		ImGui::SliderFloat("Zoom", &zoom_, 0.5f, 2.5f, "%.2f");
		ImGui::Checkbox("Floor below", &show_floor_below_);
		ImGui::TextDisabled("LMB paints. Shift erases/clears creatures. RMB inspects. MMB pan. Wheel zoom. WASD pan. Ctrl+S download.");

		const float tile_px = 32.0f * zoom_;
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const int view_w = std::max(1, static_cast<int>(avail.x / tile_px));
		const int view_h = std::max(1, static_cast<int>(avail.y / tile_px));
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImDrawList* draw = ImGui::GetWindowDrawList();
		ImGui::InvisibleButton("map_grid", ImVec2(view_w * tile_px, view_h * tile_px));
		const bool hovered = ImGui::IsItemHovered();
		const ImVec2 mouse = ImGui::GetIO().MousePos;

		for (int row = 0; row < view_h; ++row) {
			for (int col = 0; col < view_w; ++col) {
				const int x = session_.cameraX() - view_w / 2 + col;
				const int y = session_.cameraY() - view_h / 2 + row;
				const ImVec2 p0(origin.x + col * tile_px, origin.y + row * tile_px);
				const ImVec2 p1(p0.x + tile_px, p0.y + tile_px);
				draw->AddRectFilled(p0, p1, IM_COL32(20, 22, 26, 255));

				auto paintTile = [&](int z, ImU32 tint, bool overlays) {
					const rme::core::Tile* tile = session_.map().getTile(Position(x, y, z));
					if (!tile) {
						return;
					}
					if (tile->getGround()) {
						const uint16_t id = tile->getGround()->getID();
						DrawSprite(draw, p0, p1, session_.spriteIdForItem(id), FallbackColor(id), tint);
					}
					for (const auto& item : tile->getItems()) {
						DrawSprite(draw, p0, p1, session_.spriteIdForItem(item.getID()), FallbackColor(item.getID()), tint);
					}
					if (!overlays) {
						return;
					}
					if (tile->hasFlag(rme::core::TILESTATE_PROTECTIONZONE)) {
						draw->AddRectFilled(p0, p1, IM_COL32(40, 200, 80, 55));
					}
					if (tile->hasFlag(rme::core::TILESTATE_NOLOGOUT)) {
						draw->AddRectFilled(p0, p1, IM_COL32(200, 50, 50, 50));
					}
					if (tile->hasFlag(rme::core::TILESTATE_PVPZONE)) {
						draw->AddRectFilled(p0, p1, IM_COL32(220, 140, 40, 50));
					}
					if (tile->hasFlag(rme::core::TILESTATE_NOPVP)) {
						draw->AddRectFilled(p0, p1, IM_COL32(60, 90, 210, 50));
					}
					if (tile->getHouseID() != 0) {
						draw->AddRectFilled(p0, p1, IM_COL32(180, 70, 210, 70));
					}
					if (tile->hasTeleport()) {
						const ImVec2 a(p0.x + tile_px * 0.5f, p0.y + 3.0f);
						const ImVec2 b(p0.x + tile_px - 3.0f, p0.y + tile_px * 0.38f);
						const ImVec2 c(p0.x + 3.0f, p0.y + tile_px * 0.38f);
						draw->AddTriangleFilled(a, b, c, IM_COL32(40, 220, 230, 220));
					}
					if (const rme::core::Item* top = tile->topItem(); top && (top->getActionID() != 0 || top->getUniqueID() != 0)) {
						const ImVec2 pip1(p0.x + 8.0f, p0.y + 8.0f);
						draw->AddRectFilled(p0, pip1, IM_COL32(20, 20, 20, 255));
						draw->AddRectFilled(ImVec2(p0.x + 1.0f, p0.y + 1.0f), ImVec2(pip1.x - 1.0f, pip1.y - 1.0f), IM_COL32(255, 230, 40, 255));
					}
				};

				if (show_floor_below_ && session_.floor() < rme::MapMaxLayer) {
					paintTile(session_.floor() + 1, IM_COL32(255, 255, 255, 90), false);
				}
				paintTile(session_.floor(), IM_COL32_WHITE, true);
				if (x == session_.cameraX() && y == session_.cameraY()) {
					draw->AddRect(p0, ImVec2(p1.x - 1.0f, p1.y - 1.0f), IM_COL32(240, 240, 240, 220));
				}
				if (x == session_.inspect().x && y == session_.inspect().y && session_.inspect().z == session_.floor()) {
					draw->AddRect(p0, ImVec2(p1.x - 1.0f, p1.y - 1.0f), IM_COL32(255, 160, 40, 230));
				}
			}
		}

		for (const auto& spawn : session_.map().spawns()) {
			if (spawn.center.z != session_.floor()) {
				continue;
			}
			const int col = spawn.center.x - session_.cameraX() + view_w / 2;
			const int row = spawn.center.y - session_.cameraY() + view_h / 2;
			const ImVec2 center(
				origin.x + (static_cast<float>(col) + 0.5f) * tile_px,
				origin.y + (static_cast<float>(row) + 0.5f) * tile_px
			);
			draw->AddCircle(center, std::max(4.0f, static_cast<float>(spawn.radius) * tile_px), IM_COL32(250, 220, 40, 200), 40, 2.0f);
			draw->AddCircleFilled(center, 3.0f, IM_COL32(250, 220, 40, 255));
			for (const auto& creature : spawn.monsters) {
				const ImVec2 monster(
					center.x + static_cast<float>(creature.dx) * tile_px,
					center.y + static_cast<float>(creature.dy) * tile_px
				);
				draw->AddCircleFilled(monster, 3.5f, IM_COL32(220, 80, 60, 255));
				if (tile_px >= 18.0f && !creature.name.empty()) {
					char mark[2] = {creature.name.front(), 0};
					draw->AddText(ImVec2(monster.x + 4.0f, monster.y - 7.0f), IM_COL32(255, 210, 190, 230), mark);
				}
			}
		}

		auto mapToCanvas = [&](int x, int y) -> ImVec2 {
			const int col = x - session_.cameraX() + view_w / 2;
			const int row = y - session_.cameraY() + view_h / 2;
			return ImVec2(
				origin.x + (static_cast<float>(col) + 0.5f) * tile_px,
				origin.y + (static_cast<float>(row) + 0.5f) * tile_px
			);
		};
		for (const auto& [_, tile] : session_.map().tiles()) {
			const rme::core::Item* portal = tile.firstTeleport();
			if (!portal || tile.getPosition().z != session_.floor()) {
				continue;
			}
			const Position dest = portal->getDestination();
			if (dest.z != session_.floor()) {
				continue;
			}
			draw->AddLine(mapToCanvas(tile.getPosition().x, tile.getPosition().y), mapToCanvas(dest.x, dest.y), IM_COL32(40, 220, 230, 160), 2.0f);
		}

		auto tileFromMouse = [&](Position& out) -> bool {
			const int col = static_cast<int>((mouse.x - origin.x) / tile_px);
			const int row = static_cast<int>((mouse.y - origin.y) / tile_px);
			if (col < 0 || row < 0 || col >= view_w || row >= view_h) {
				return false;
			}
			out = Position(
				session_.cameraX() - view_w / 2 + col,
				session_.cameraY() - view_h / 2 + row,
				session_.floor()
			);
			return session_.map().inBounds(out);
		};

		Position hover_pos;
		const bool have_hover = hovered && tileFromMouse(hover_pos);
		if (have_hover) {
			if (const rme::core::Tile* hover_tile = session_.map().getTile(hover_pos)) {
				ImGui::BeginTooltip();
				ImGui::Text("%d, %d, %d", hover_pos.x, hover_pos.y, hover_pos.z);
				if (hover_tile->getGround()) {
					const auto* type = session_.items().get(hover_tile->getGround()->getID());
					ImGui::Text("ground %u %s", hover_tile->getGround()->getID(), type ? type->name.c_str() : "");
				}
				for (const auto& item : hover_tile->getItems()) {
					const auto* type = session_.items().get(item.getID());
					ImGui::Text("item %u %s", item.getID(), type ? type->name.c_str() : "");
					if (item.getActionID() || item.getUniqueID()) {
						ImGui::Text("  aid %u  uid %u", item.getActionID(), item.getUniqueID());
					}
					if (!item.getText().empty()) {
						ImGui::Text("  \"%s\"", item.getText().c_str());
					}
					if (item.hasDestination()) {
						ImGui::Text("  dest %d,%d,%d", item.getDestination().x, item.getDestination().y, item.getDestination().z);
					}
					if (!item.getContents().empty()) {
						ImGui::Text("  contains %zu", item.getContents().size());
					}
				}
				if (hover_tile->getHouseID()) {
					ImGui::Text("house %u", hover_tile->getHouseID());
				}
				for (const std::string& name : session_.creaturesAt(hover_pos)) {
					ImGui::Text("creature %s", name.c_str());
				}
				ImGui::EndTooltip();
			} else if (!session_.creaturesAt(hover_pos).empty()) {
				ImGui::BeginTooltip();
				ImGui::Text("%d, %d, %d", hover_pos.x, hover_pos.y, hover_pos.z);
				for (const std::string& name : session_.creaturesAt(hover_pos)) {
					ImGui::Text("creature %s", name.c_str());
				}
				ImGui::EndTooltip();
			}
		}
		if (have_hover && session_.brushKind() != rme::core::BrushKind::Select && session_.brushKind() != rme::core::BrushKind::Fill) {
			for (const Position& cell : session_.hoverFootprint(hover_pos)) {
				const int col = cell.x - session_.cameraX() + view_w / 2;
				const int row = cell.y - session_.cameraY() + view_h / 2;
				if (col < 0 || row < 0 || col >= view_w || row >= view_h) {
					continue;
				}
				const ImVec2 p0(origin.x + col * tile_px, origin.y + row * tile_px);
				draw->AddRect(p0, ImVec2(p0.x + tile_px, p0.y + tile_px), IM_COL32(250, 230, 80, 160));
			}
		}

		if (session_.selection().visible()) {
			for (const Position& cell : session_.selection().tiles()) {
				const int col = cell.x - session_.cameraX() + view_w / 2;
				const int row = cell.y - session_.cameraY() + view_h / 2;
				if (col < 0 || row < 0 || col >= view_w || row >= view_h) {
					continue;
				}
				const ImVec2 p0(origin.x + col * tile_px, origin.y + row * tile_px);
				draw->AddRectFilled(p0, ImVec2(p0.x + tile_px, p0.y + tile_px), IM_COL32(80, 160, 255, 50));
				draw->AddRect(p0, ImVec2(p0.x + tile_px, p0.y + tile_px), IM_COL32(90, 180, 255, 220));
			}
		}

		if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
			const float factor = ImGui::GetIO().MouseWheel > 0.0f ? 1.15f : (1.0f / 1.15f);
			zoom_ = std::clamp(zoom_ * factor, 0.5f, 2.5f);
		}

		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
			panning_ = true;
			pan_last_ = mouse;
		}
		if (panning_ && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
			const int dx = static_cast<int>((pan_last_.x - mouse.x) / tile_px);
			const int dy = static_cast<int>((pan_last_.y - mouse.y) / tile_px);
			if (dx != 0 || dy != 0) {
				session_.panBy(dx, dy);
				pan_last_ = mouse;
			}
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
			panning_ = false;
		}

		if (have_hover && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			session_.pickAt(hover_pos);
		}

		const rme::core::BrushKind tool = session_.brushKind();
		if (!panning_ && have_hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			if (tool == rme::core::BrushKind::Select) {
				session_.endStroke();
				session_.selection().begin(hover_pos);
			} else if (tool == rme::core::BrushKind::Fill) {
				session_.fillAt(hover_pos);
			} else {
				session_.beginStroke();
				session_.strokeAt(hover_pos, ImGui::GetIO().KeyShift);
			}
		} else if (session_.selection().dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			if (have_hover) {
				session_.selection().update(hover_pos);
			}
		} else if (!panning_ && session_.isStroking() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && have_hover) {
			session_.strokeAt(hover_pos, ImGui::GetIO().KeyShift);
		}

		if (session_.selection().dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			session_.selection().finish();
		}
		if (session_.isStroking() && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			session_.endStroke();
		}
		ImGui::End();
	}

	void DrawBrushTools() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 750.0f, viewport->WorkPos.y + 460.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(380.0f, 320.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Brushes", &show_brushes_)) {
			ImGui::End();
			return;
		}

		const rme::core::BrushKind kinds[] = {
			rme::core::BrushKind::Auto,
			rme::core::BrushKind::Ground,
			rme::core::BrushKind::Overlay,
			rme::core::BrushKind::Eraser,
			rme::core::BrushKind::Fill,
			rme::core::BrushKind::Select,
			rme::core::BrushKind::Flags,
			rme::core::BrushKind::House,
			rme::core::BrushKind::Wall,
			rme::core::BrushKind::Border,
			rme::core::BrushKind::Doodad,
			rme::core::BrushKind::Door,
			rme::core::BrushKind::Table,
			rme::core::BrushKind::Carpet,
			rme::core::BrushKind::Creature,
		};
		for (int i = 0; i < 15; ++i) {
			if (i > 0 && i % 6 == 0) {
				// New row every 6 tools
			} else if (i > 0) {
				ImGui::SameLine();
			}
			const bool selected = session_.brushKind() == kinds[i];
			if (selected) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.62f, 0.38f, 1.0f));
			}
			if (ImGui::Button(rme::core::BrushKindName(kinds[i]))) {
				session_.setBrushKind(kinds[i]);
			}
			if (selected) {
				ImGui::PopStyleColor();
			}
		}

		int size = session_.brushSize();
		if (ImGui::SliderInt("Size", &size, 1, 9)) {
			session_.setBrushSize(size);
		}

		ImGui::TextUnformatted("Flag mask");
		const uint32_t flag_choices[] = {
			rme::core::TILESTATE_PROTECTIONZONE,
			rme::core::TILESTATE_NOPVP,
			rme::core::TILESTATE_NOLOGOUT,
			rme::core::TILESTATE_PVPZONE,
		};
		const char* flag_labels[] = { "PZ", "NoPvP", "NoLogout", "PvP" };
		for (int i = 0; i < 4; ++i) {
			if (i > 0) {
				ImGui::SameLine();
			}
			if (ImGui::RadioButton(flag_labels[i], session_.flagMask() == flag_choices[i])) {
				session_.setFlagMask(flag_choices[i]);
			}
		}

		if (session_.brushKind() == rme::core::BrushKind::Creature) {
			if (ImGui::InputText("Creature", monster_name_, sizeof(monster_name_))) {
				session_.setCreatureName(monster_name_);
			}
			int radius = session_.spawnRadius();
			if (ImGui::SliderInt("Spawn radius", &radius, 1, 16)) {
				session_.setSpawnRadius(radius);
				spawn_radius_ = radius;
			}
			int delay = static_cast<int>(session_.spawnTime());
			if (ImGui::InputInt("Spawntime", &delay)) {
				session_.setSpawnTime(static_cast<uint32_t>(std::max(1, delay)));
			}
			ImGui::TextUnformatted("Names");
			for (int i = 0; i < 8; ++i) {
				if (i > 0 && i % 4 != 0) {
					ImGui::SameLine();
				}
				if (ImGui::SmallButton(rme::core::kSampleCreatures[i])) {
					session_.setCreatureName(rme::core::kSampleCreatures[i]);
					std::snprintf(monster_name_, sizeof(monster_name_), "%s", rme::core::kSampleCreatures[i]);
				}
			}
		}

		ImGui::Text("Resolved: %s", rme::core::BrushKindName(session_.resolvedBrush()));
		ImGui::Text("House id %u", session_.houseId());
		ImGui::Text("Creature %s  r=%d", session_.creatureName().c_str(), session_.spawnRadius());
		ImGui::Text("Selection %zu   Clipboard %zu", session_.selection().size(), session_.clipboardSize());
		if (ImGui::Button("Delete sel")) {
			session_.deleteSelection();
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy")) {
			session_.copySelection();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cut")) {
			session_.cutSelection();
		}
		ImGui::SameLine();
		if (ImGui::Button("Paste") && session_.hasClipboard()) {
			session_.pasteAt(Position(session_.cameraX(), session_.cameraY(), session_.floor()));
		}
		ImGui::TextDisabled("Fill replaces 4-connected tiles with the same ground id.");
		ImGui::TextDisabled("Flags paints PZ/PvP bits (green overlay). Shift+click clears.");
		ImGui::TextDisabled("House paints magenta house tiles for the selected house id.");
		ImGui::TextDisabled("Wall auto-connects timber pieces. Shift+click removes the family.");
		ImGui::TextDisabled("Border paints water shores against land. Shift+click removes water.");
		ImGui::TextDisabled("Doodad scatters overlays (60%% flowers). Shift+click removes the family.");
		ImGui::TextDisabled("Door faces the nearby wall. Table and carpet auto-connect. Shift+click clears.");
		ImGui::TextDisabled("Creature paints spawn.xml monsters. Shift+click removes that tile's creature.");
		ImGui::TextDisabled("[ ] change size. Del deletes. Ctrl+C / X / V clipboard.");
		ImGui::End();
	}

	void DrawItemPalette() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 750.0f, viewport->WorkPos.y + 28.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Item palette", &show_palette_)) {
			ImGui::End();
			return;
		}

		ImGui::Text("%zu items  |  brush %u", session_.items().size(), session_.brushId());
		const auto& tilesets = session_.materials().tilesets();
		if (!tilesets.empty()) {
			if (palette_tileset_ < 0 || palette_tileset_ >= static_cast<int>(tilesets.size())) {
				palette_tileset_ = 0;
			}
			if (ImGui::BeginCombo("Tileset", tilesets[static_cast<std::size_t>(palette_tileset_)].name.c_str())) {
				for (int i = 0; i < static_cast<int>(tilesets.size()); ++i) {
					const bool selected = i == palette_tileset_;
					if (ImGui::Selectable(tilesets[static_cast<std::size_t>(i)].name.c_str(), selected)) {
						palette_tileset_ = i;
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
		ImGui::Separator();

		std::vector<uint16_t> ids;
		if (!tilesets.empty()) {
			ids = tilesets[static_cast<std::size_t>(palette_tileset_)].items;
		} else {
			for (const auto& type : session_.items().items()) {
				ids.push_back(type.id);
			}
		}

		const float cell = 40.0f;
		const float spacing = 6.0f;
		const float avail = ImGui::GetContentRegionAvail().x;
		const int columns = std::max(1, static_cast<int>((avail + spacing) / (cell + spacing)));
		int index = 0;
		for (uint16_t id : ids) {
			const auto* type = session_.items().get(id);
			if (!type) {
				continue;
			}
			if (index % columns != 0) {
				ImGui::SameLine(0.0f, spacing);
			}
			ImGui::PushID(type->id);
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			const ImVec2 p1(p0.x + cell, p0.y + cell);
			if (ImGui::InvisibleButton("item", ImVec2(cell, cell))) {
				session_.setBrushId(type->id);
			}
			ImDrawList* draw = ImGui::GetWindowDrawList();
			draw->AddRectFilled(p0, p1, IM_COL32(18, 20, 22, 255));
			DrawSprite(draw, p0, p1, type->sprite_id, FallbackColor(type->id));
			if (type->id == session_.brushId()) {
				draw->AddRect(p0, p1, IM_COL32(250, 230, 80, 255), 0.0f, 0, 2.0f);
			} else if (ImGui::IsItemHovered()) {
				draw->AddRect(p0, p1, IM_COL32(220, 220, 220, 180));
			}
			if (ImGui::IsItemHovered()) {
				const bool is_wall = session_.materials().wallForItem(type->id) != nullptr;
				const bool is_border = session_.materials().borderForItem(type->id) != nullptr;
				const bool is_doodad = session_.materials().doodadForItem(type->id) != nullptr;
				ImGui::SetTooltip("%u %s\nsprite %u%s%s%s%s%s%s",
					type->id,
					type->name.c_str(),
					type->sprite_id,
					type->ground ? "\nground" : "",
					type->not_walkable ? "\nnot walkable" : "",
					type->pickupable ? "\npickupable" : "",
					is_wall ? "\nwall set" : "",
					is_border ? "\nborder set" : "",
					is_doodad ? "\ndoodad set" : "");
			}
			ImGui::PopID();
			++index;
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
	bool show_assets_ = false;
	bool show_log_ = false;
	bool show_about_ = false;
	bool show_new_map_ = false;
	bool show_map_props_ = false;
	bool show_canvas_ = true;
	bool show_inspector_ = true;
	bool show_palette_ = true;
	bool show_brushes_ = true;
	bool show_minimap_ = true;
	bool show_markers_ = true;
	bool show_houses_ = true;
	bool show_spawns_ = true;
	bool show_goto_ = false;
	bool show_properties_ = true;
	bool show_find_ = true;
	bool show_issues_ = true;
	bool show_floor_below_ = true;
	int palette_tileset_ = 0;
	bool follow_camera_ = false;
	float zoom_ = 1.0f;
	bool panning_ = false;
	ImVec2 pan_last_{};
	unsigned int minimap_tex_ = 0;
	int minimap_tex_w_ = 0;
	int minimap_tex_h_ = 0;
	char town_name_[64] = "New town";
	char waypoint_name_[64] = "Waypoint";
	char house_name_[64] = "House";
	char monster_name_[64] = "Rat";
	int spawn_radius_ = 3;
	int goto_x_ = 100;
	int goto_y_ = 100;
	int goto_z_ = 7;
	int prop_aid_ = 0;
	int prop_uid_ = 0;
	int prop_count_ = 1;
	int prop_charges_ = 0;
	int prop_depot_ = 0;
	int prop_door_ = 0;
	int prop_dx_ = 0;
	int prop_dy_ = 0;
	int prop_dz_ = 7;
	bool prop_has_dest_ = false;
	char prop_text_[128] = "";
	char prop_desc_[128] = "";
	uint64_t props_stamp_ = 0;
	int find_id_ = 0;
	int find_aid_ = 0;
	int find_uid_ = 0;
	bool find_teleports_ = false;
	int container_add_id_ = 104;
	ImVec4 clear_color_ = ImVec4(0.07f, 0.08f, 0.09f, 1.00f);
	char fetch_url_[512] = "";
	char new_map_name_[128] = "Untitled.otbm";
	char map_name_[128] = "Untitled.otbm";
	char map_desc_[512] = "";
	int new_map_w_ = 256;
	int new_map_h_ = 256;
	std::string last_save_message_;
	rme::core::EditorSession session_;
	rme::gfx::SpriteAtlas atlas_;
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
