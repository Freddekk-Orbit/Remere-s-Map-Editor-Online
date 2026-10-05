#include "asset_bridge.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <utility>

#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
	#include <emscripten/fetch.h>
#endif

namespace rme::wasm {
namespace {

std::mutex g_mutex;
std::vector<VirtualFile> g_files;
std::vector<std::string> g_log;
bool g_vfs_ready = false;
bool g_persist_ready = false;
int g_next_fetch_id = 1;

struct PendingFetch {
	std::string dest_path;
	FetchCallback callback;
};

std::unordered_map<int, PendingFetch> g_fetches;

void LogUnlocked(std::string message) {
	g_log.push_back(std::move(message));
	if (g_log.size() > 64) {
		g_log.erase(g_log.begin(), g_log.begin() + static_cast<std::ptrdiff_t>(g_log.size() - 64));
	}
}

void Log(std::string message) {
	std::lock_guard lock(g_mutex);
	LogUnlocked(std::move(message));
}

void EnsureDirectory(const char* path) {
	std::error_code ec;
	std::filesystem::create_directories(path, ec);
}

void UpsertFileUnlocked(const std::string& vfs_path, std::size_t size, bool persistent) {
	const auto name = std::filesystem::path(vfs_path).filename().string();
	for (auto& file : g_files) {
		if (file.vfs_path == vfs_path) {
			file.size = size;
			file.kind = GuessAssetKind(name);
			file.persistent = persistent || file.persistent;
			file.name = name;
			return;
		}
	}
	g_files.push_back(VirtualFile{
		.vfs_path = vfs_path,
		.name = name,
		.size = size,
		.kind = GuessAssetKind(name),
		.persistent = persistent,
	});
}

void ScanDirectoryUnlocked(const char* dir, bool persistent) {
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec)) {
		return;
	}
	for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
		if (!entry.is_regular_file(ec)) {
			continue;
		}
		const auto path = entry.path().generic_string();
		const auto size = static_cast<std::size_t>(entry.file_size(ec));
		UpsertFileUnlocked(path, size, persistent);
	}
}

#ifdef __EMSCRIPTEN__
void OnFetchSuccess(emscripten_fetch_t* fetch) {
	PendingFetch pending;
	{
		std::lock_guard lock(g_mutex);
		const auto it = g_fetches.find(static_cast<int>(fetch->userData ? reinterpret_cast<intptr_t>(fetch->userData) : 0));
		if (it != g_fetches.end()) {
			pending = std::move(it->second);
			g_fetches.erase(it);
		}
	}

	const bool wrote = WriteMemoryFile(pending.dest_path, fetch->data, static_cast<std::size_t>(fetch->numBytes));
	emscripten_fetch_close(fetch);
	if (pending.callback) {
		pending.callback(wrote, pending.dest_path, wrote ? std::string() : std::string("Failed to write fetched file"));
	}
}

void OnFetchError(emscripten_fetch_t* fetch) {
	PendingFetch pending;
	{
		std::lock_guard lock(g_mutex);
		const auto it = g_fetches.find(static_cast<int>(fetch->userData ? reinterpret_cast<intptr_t>(fetch->userData) : 0));
		if (it != g_fetches.end()) {
			pending = std::move(it->second);
			g_fetches.erase(it);
		}
	}

	char error[160];
	std::snprintf(error, sizeof(error), "HTTP fetch failed (status %d)", fetch->status);
	emscripten_fetch_close(fetch);
	Log(error);
	if (pending.callback) {
		pending.callback(false, pending.dest_path, error);
	}
}
#endif

} // namespace

AssetKind GuessAssetKind(const std::string& name) {
	auto lower = name;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	if (lower.ends_with(".otbm")) {
		return AssetKind::Otbm;
	}
	if (lower.ends_with(".dat")) {
		return AssetKind::Dat;
	}
	if (lower.ends_with(".spr")) {
		return AssetKind::Spr;
	}
	if (lower.ends_with(".xml")) {
		return AssetKind::Xml;
	}
	if (lower.ends_with(".otgz")) {
		return AssetKind::Otgz;
	}
	return lower.find('.') == std::string::npos ? AssetKind::Unknown : AssetKind::Other;
}

void InitVirtualFileSystem() {
	EnsureDirectory(kUploadDir);
	EnsureDirectory(kAssetDir);
	EnsureDirectory(kPersistDir);

	{
		std::lock_guard lock(g_mutex);
		g_vfs_ready = true;
		LogUnlocked("Virtual filesystem mounted (MEMFS).");
	}

#ifdef __EMSCRIPTEN__
	EM_ASM({
		try { FS.mkdir('/uploads'); } catch (e) {}
		try { FS.mkdir('/assets'); } catch (e) {}
		try { FS.mkdir('/persist'); } catch (e) {}
		try {
			FS.mount(IDBFS, {}, '/persist');
			FS.syncfs(true, function(err) {
				var ok = (!err) ? 0 : 1;
				if (typeof Module.ccall === 'function') {
					Module.ccall('rme_on_idbfs_ready', null, ['number'], [ok]);
				}
			});
		} catch (e) {
			console.error('IDBFS mount failed', e);
			if (typeof Module.ccall === 'function') {
				Module.ccall('rme_on_idbfs_ready', null, ['number'], [1]);
			}
		}
	});
#else
	NotifyPersistentReady(true);
#endif
}

void SyncPersistentStore() {
#ifdef __EMSCRIPTEN__
	EM_ASM({
		FS.syncfs(false, function(err) {
			if (err) {
				console.error('IDBFS sync failed', err);
			}
		});
	});
#endif
}

bool IsFileSystemReady() {
	std::lock_guard lock(g_mutex);
	return g_vfs_ready;
}

bool IsPersistentReady() {
	std::lock_guard lock(g_mutex);
	return g_persist_ready;
}

void NotifyFileUploaded(const std::string& vfs_path, std::size_t size) {
	{
		std::lock_guard lock(g_mutex);
		UpsertFileUnlocked(vfs_path, size, false);
		LogUnlocked("Imported " + vfs_path + " (" + std::to_string(size) + " bytes)");
	}
}

void NotifyPersistentReady(bool ok) {
	std::lock_guard lock(g_mutex);
	g_persist_ready = ok;
	if (ok) {
		ScanDirectoryUnlocked(kPersistDir, true);
		LogUnlocked("IDBFS ready — persistent files restored.");
	} else {
		LogUnlocked("IDBFS unavailable; session files will stay in MEMFS.");
	}
}

bool WriteMemoryFile(const std::string& vfs_path, const void* data, std::size_t size) {
	const auto parent = std::filesystem::path(vfs_path).parent_path();
	std::error_code ec;
	if (!parent.empty()) {
		std::filesystem::create_directories(parent, ec);
	}

	FILE* file = std::fopen(vfs_path.c_str(), "wb");
	if (!file) {
		Log("Failed to open " + vfs_path + " for writing");
		return false;
	}
	const auto written = std::fwrite(data, 1, size, file);
	std::fclose(file);
	if (written != size) {
		Log("Short write to " + vfs_path);
		return false;
	}

	{
		std::lock_guard lock(g_mutex);
		const bool persistent = vfs_path.rfind(kPersistDir, 0) == 0;
		UpsertFileUnlocked(vfs_path, size, persistent);
		LogUnlocked("Wrote " + vfs_path + " (" + std::to_string(size) + " bytes)");
	}
	if (vfs_path.rfind(kPersistDir, 0) == 0) {
		SyncPersistentStore();
	}
	return true;
}

bool FileExists(const std::string& vfs_path) {
	std::error_code ec;
	return std::filesystem::exists(vfs_path, ec);
}

std::size_t FileSize(const std::string& vfs_path) {
	std::error_code ec;
	if (!std::filesystem::exists(vfs_path, ec)) {
		return 0;
	}
	return static_cast<std::size_t>(std::filesystem::file_size(vfs_path, ec));
}

std::vector<VirtualFile> ListVirtualFiles() {
	std::lock_guard lock(g_mutex);
	ScanDirectoryUnlocked(kUploadDir, false);
	ScanDirectoryUnlocked(kAssetDir, false);
	ScanDirectoryUnlocked(kPersistDir, true);
	return g_files;
}

std::vector<std::string> ConsumeLog() {
	std::lock_guard lock(g_mutex);
	return g_log;
}

void FetchAsset(const std::string& url, const std::string& dest_path, FetchCallback callback) {
#ifdef __EMSCRIPTEN__
	int id = 0;
	{
		std::lock_guard lock(g_mutex);
		id = g_next_fetch_id++;
		g_fetches[id] = PendingFetch{dest_path, std::move(callback)};
		LogUnlocked("Fetching " + url + " -> " + dest_path);
	}

	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	std::strcpy(attr.requestMethod, "GET");
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attr.onsuccess = OnFetchSuccess;
	attr.onerror = OnFetchError;
	attr.userData = reinterpret_cast<void*>(static_cast<intptr_t>(id));
	emscripten_fetch(&attr, url.c_str());
#else
	(void)url;
	if (callback) {
		callback(false, dest_path, "FETCH is only available in the Emscripten build");
	}
#endif
}

void OpenBrowserFilePicker() {
#ifdef __EMSCRIPTEN__
	EM_ASM({
		var input = document.getElementById('rme-file-input');
		if (input) {
			input.click();
		}
	});
#endif
}

bool PersistUploadedFile(const std::string& vfs_path) {
	if (!FileExists(vfs_path)) {
		return false;
	}
	const auto dest = std::string(kPersistDir) + "/" + std::filesystem::path(vfs_path).filename().string();
	std::error_code ec;
	std::filesystem::copy_file(vfs_path, dest, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec) {
		Log("Could not persist " + vfs_path + ": " + ec.message());
		return false;
	}
	{
		std::lock_guard lock(g_mutex);
		UpsertFileUnlocked(dest, FileSize(dest), true);
		LogUnlocked("Copied " + vfs_path + " to IDBFS " + dest);
	}
	SyncPersistentStore();
	return true;
}

} // namespace rme::wasm

#ifdef __EMSCRIPTEN__
extern "C" {

EMSCRIPTEN_KEEPALIVE
void rme_on_file_uploaded(const char* path, int size) {
	if (!path) {
		return;
	}
	rme::wasm::NotifyFileUploaded(path, static_cast<std::size_t>(size));
}

EMSCRIPTEN_KEEPALIVE
void rme_on_idbfs_ready(int error_flag) {
	rme::wasm::NotifyPersistentReady(error_flag == 0);
}

} // extern "C"
#endif
