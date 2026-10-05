#include "platform/platform.h"
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
		return true;
	}

	bool Frame() {
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
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.13f, 0.96f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.16f, 0.14f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.28f, 0.20f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.16f, 0.32f, 0.22f, 0.80f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.36f, 0.24f, 0.80f);
	}

	void DrawUi() {
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Upload client / map files...")) {
					rme::wasm::OpenBrowserFilePicker();
				}
				if (ImGui::MenuItem("Fetch Tibia.dat from /assets URL", nullptr, false, fetch_url_[0] != '\0')) {
					StartSampleFetch();
				}
				ImGui::Separator();
				ImGui::MenuItem("Quit is handled by the browser tab", nullptr, false, false);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View")) {
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

		if (show_about_) {
			DrawAbout();
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
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 16.0f, viewport->WorkPos.y + 36.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420.0f, 220.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("RME Wasm — Phase 1", nullptr, ImGuiWindowFlags_NoCollapse)) {
			ImGui::End();
			return;
		}

		ImGui::TextWrapped(
			"wxWidgets has been decoupled from the browser entry point. "
			"This loop is Dear ImGui + SDL2 + WebGL2, driven by emscripten_set_main_loop()."
		);
		ImGui::Separator();
		ImGui::Text("VFS ready:      %s", rme::wasm::IsFileSystemReady() ? "yes" : "no");
		ImGui::Text("IDBFS persist:  %s", rme::wasm::IsPersistentReady() ? "yes" : "pending / unavailable");
		ImGui::Text("Uploaded files: %d", static_cast<int>(rme::wasm::ListVirtualFiles().size()));
		ImGui::Spacing();
		if (ImGui::Button("Open file picker")) {
			rme::wasm::OpenBrowserFilePicker();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("or drop .otbm / .dat / .spr on the page");
		ImGui::End();
	}

	void DrawAbout() {
		ImGui::SetNextWindowSize(ImVec2(520.0f, 280.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("About Remere's Map Editor (Wasm)", &show_about_)) {
			ImGui::End();
			return;
		}
		ImGui::TextUnformatted("Remere's Map Editor — WebAssembly port");
		ImGui::Separator();
		ImGui::TextWrapped(
			"Phase 1 establishes the Emscripten build, replaces wxApp::OnRun() with a "
			"browser main loop, and mounts MEMFS/IDBFS so client files can be imported "
			"without desktop disk I/O. Map algorithms stay in C++; only the windowing "
			"and file-bridge layers are new."
		);
		ImGui::Spacing();
		ImGui::BulletText("UI: Dear ImGui (SDL2 + OpenGL ES 3.0 / WebGL2)");
		ImGui::BulletText("Build: emcmake + CMake");
		ImGui::BulletText("wxWidgets: stubbed via src/wx_stub include path");
		ImGui::End();
	}

	void DrawAssetBrowser() {
		ImGui::SetNextWindowSize(ImVec2(560.0f, 360.0f), ImGuiCond_FirstUseEver);
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
		ImGui::SetNextWindowSize(ImVec2(480.0f, 220.0f), ImGuiCond_FirstUseEver);
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
	bool show_log_ = true;
	bool show_about_ = true;
	ImVec4 clear_color_ = ImVec4(0.07f, 0.08f, 0.09f, 1.00f);
	char fetch_url_[512] = "";
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
