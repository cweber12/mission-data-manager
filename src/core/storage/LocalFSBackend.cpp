#include "LocalFSBackend.hpp"
#include <filesystem>
#include <fstream>

std::string LocalFSBackend::put(const std::string& mission_id,
                                const std::string& id,
                                std::string_view bytes) {
  namespace fs = std::filesystem;
  fs::path dir = fs::path(hotRoot_) / mission_id;
  fs::create_directories(dir);
  fs::path file = dir / id; // no extension; logical_name is in DB
  std::ofstream os(file, std::ios::binary);
  os.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  os.flush();
  return fs::weakly_canonical(file).string();
}
