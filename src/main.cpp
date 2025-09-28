// src/main.cpp
#include <cstdlib>
#include <string>
#include <iostream>
#include <filesystem>
#include <stdexcept>

#include "core/metadata/InitDb.hpp"
#include "core/metadata/MetadataStore.hpp"
#include "core/storage/LocalFSBackend.hpp"
#include "services/api/HttpServer.hpp"

// ---------- helpers ----------

static std::string get_env_or(const char* key, const std::string& defval) {
#ifdef _WIN32
  size_t len = 0;
  char* buf = nullptr;
  if (_dupenv_s(&buf, &len, key) == 0 && buf) {
    std::string v(buf);
    free(buf);
    return v;
  }
  return defval;
#else
  if (const char* v = std::getenv(key)) return std::string(v);
  return defval;
#endif
}

static std::string defaultDbPath() {
  return get_env_or("MDM_DB_PATH", "data/mission-metadata.db");
}

// Look for schema.sql in CWD first (CI copies it there), then fallback.
static std::string findSchemaPath() {
  namespace fs = std::filesystem;
  const fs::path candidates[] = {
    fs::current_path() / "schema.sql",
    fs::path("src/core/metadata/schema.sql")
  };
  for (const auto& p : candidates) {
    if (fs::exists(p)) return p.string();
  }
  throw std::runtime_error("schema.sql not found (looked in build/ and src/core/metadata)");
}

static int envPortOrDefault() {
  try {
    return std::stoi(get_env_or("MDM_PORT", "8080"));
  } catch (...) {
    return 8080;
  }
}

static void ensure_dirs_for(const std::string& file_path) {
  namespace fs = std::filesystem;
  fs::path parent = fs::path(file_path).parent_path();
  if (!parent.empty()) fs::create_directories(parent);
}

static void print_usage(const char* argv0) {
  std::cout << "Usage:\n"
            << "  " << argv0 << " --init        # create/upgrade SQLite schema\n"
            << "  " << argv0 << " --serve       # start HTTP server (MDM_PORT or 8080)\n";
}

// ---------- main ----------

int main(int argc, char** argv) {
  try {
    if (argc > 1 && std::string(argv[1]) == "--init") {
      const std::string dbPath = defaultDbPath();
      const std::string schemaPath = findSchemaPath();
      ensure_dirs_for(dbPath);
      initDatabase(dbPath, schemaPath);
      std::cout << "DB initialized at: " << dbPath << "\n";
      return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "--serve") {
      // Self-heal DB on startup (idempotent)
      const std::string dbPath = defaultDbPath();
      const std::string schemaPath = findSchemaPath();
      ensure_dirs_for(dbPath);
      initDatabase(dbPath, schemaPath);

      // Storage roots (can be extended to env/config later)
      const std::string hotRoot  = get_env_or("MDM_HOT_ROOT",  "data/hot");
      const std::string coldRoot = get_env_or("MDM_COLD_ROOT", "data/cold");
      std::filesystem::create_directories(hotRoot);
      std::filesystem::create_directories(coldRoot);

      // Construct services
      MetadataStore store(dbPath);
      LocalFSBackend fs(hotRoot, coldRoot);

      // Port + (optional) API key
      const int port = envPortOrDefault();
      const std::string apiKey = get_env_or("MDM_API_KEY", ""); // empty = auth disabled (early integration)

      // Start server (expects the expanded signature)
      mdm::run_http_server(store, fs, port, apiKey);
      return 0;
    }

    print_usage(argv[0]);
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 2;
  }
}

