#pragma once

// WASM-adapted subset of upstream source/definitions.h.
// Keeps RME version macros and helpers without wxWidgets.

#include <cassert>
#include <cstdint>
#include <string>

#define __W_RME_APPLICATION_NAME__ "Remere's Map Editor (Wasm)"
#define __RME_VERSION_MAJOR__ 4
#define __RME_VERSION_MINOR__ 0
#define __RME_SUBVERSION__ 0

#define OTGZ_SUPPORT 0
#define CLIENT_VERSION 1100
#define ASSETS_NAME "Tibia"

#ifndef FORCEINLINE
	#define FORCEINLINE inline
#endif

#ifndef newd
	#define newd new
#endif

#ifndef ASSERT
	#define ASSERT assert
#endif

#ifndef NDEBUG
	#define NDEBUG 1
#endif

inline std::string i2s(int i) {
	return std::to_string(i);
}

#define IMPLEMENT_INCREMENT_OP(Type)                 \
	inline Type& operator++(Type& type) {            \
		type = static_cast<Type>(static_cast<int>(type) + 1); \
		return type;                                 \
	}
