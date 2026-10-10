#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace rme::wasm {

enum class AssetKind {
	Unknown,
	Otbm,
	Dat,
	Spr,
	Xml,
	Otgz,
	Other,
};

struct VirtualFile {
	std::string vfs_path;
	std::string name;
	std::size_t size = 0;
	AssetKind kind = AssetKind::Unknown;
	bool persistent = false;
};

using FetchCallback = std::function<void(bool ok, const std::string& vfs_path, const std::string& error)>;

// MEMFS /uploads  — files dropped or chosen in the browser (session only)
// MEMFS /assets   — fetched or preloaded client data
// IDBFS /persist  — survives reloads once synced
inline constexpr const char* kUploadDir = "/uploads";
inline constexpr const char* kAssetDir = "/assets";
inline constexpr const char* kPersistDir = "/persist";

void InitVirtualFileSystem();
void SyncPersistentStore();
bool IsFileSystemReady();
bool IsPersistentReady();

void NotifyFileUploaded(const std::string& vfs_path, std::size_t size);
void NotifyPersistentReady(bool ok);

bool WriteMemoryFile(const std::string& vfs_path, const void* data, std::size_t size);
bool FileExists(const std::string& vfs_path);
std::size_t FileSize(const std::string& vfs_path);
std::vector<VirtualFile> ListVirtualFiles();
std::vector<std::string> ConsumeLog();

AssetKind GuessAssetKind(const std::string& name);
void FetchAsset(const std::string& url, const std::string& dest_path, FetchCallback callback);
void OpenBrowserFilePicker();
bool PersistUploadedFile(const std::string& vfs_path);
void ForgetPersistedClientAssets();
bool DownloadVfsFile(const std::string& vfs_path, const std::string& filename);

} // namespace rme::wasm
