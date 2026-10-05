#pragma once

// Shared build switches for the RME Wasm port.
// Prefer these macros over sprinkling #ifdef __EMSCRIPTEN__ through core logic.

#if defined(__EMSCRIPTEN__)
	#define RME_PLATFORM_WASM 1
	#define RME_USE_IMGUI 1
	#define RME_USE_WXWIDGETS 0
#else
	#ifndef RME_PLATFORM_WASM
		#define RME_PLATFORM_WASM 0
	#endif
	#ifndef RME_USE_IMGUI
		#define RME_USE_IMGUI 1
	#endif
	#ifndef RME_USE_WXWIDGETS
		#define RME_USE_WXWIDGETS 0
	#endif
#endif

#if RME_USE_WXWIDGETS && RME_PLATFORM_WASM
	#error "wxWidgets cannot be enabled for the WebAssembly target"
#endif
