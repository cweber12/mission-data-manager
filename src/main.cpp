// src/main.cpp
#include <cstdlib>
#include <string>
#include <iostream>
#include <filesystem>
#include "core/metadata/InitDb.hpp"
#include "services/api/HttpServer.hpp"

static std::string defaultDbPath() {
  if (const char* p = std::getenv("MDM_DB_PATH")) return std::string(p);
  return "data/mission-metadata.db";
}

// Look for schema.sql in CWD first (CI copies it there), then fallback.
static std::string findSchemaPath() {
  namespace fs = std::filesystem;
  const fs::path candidates[] = {
    fs::current_path() / "schema.sql",
    fs::path("src/core/metadata/schema.sql")
  };
  for (const auto& p : candidates) if (fs::exists(p)) return p.string();
  throw std::runtime_error("schema.sql not found (looked in build/ and src/core/metadata)");
}

static int envPortOrDefault() {
  if (const char* p = std::getenv("MDM_PORT")) return std::atoi(p);
  return 8080;
}

static void print_usage(const char* argv0) {
  std::cout << "Usage:\n"
            << "  " << argv0 << " --init        # create/upgrade SQLite schema\n"
            << "  " << argv0 << " --serve       # start HTTP server (MDM_PORT or 8080)\n";
}

int main(int argc, char** argv) {
  try {
    if (argc > 1 && std::string(argv[1]) == "--init") {
      const std::string dbPath = defaultDbPath();
      const std::string schemaPath = findSchemaPath();
      std::filesystem::create_directories(std::filesystem::path(dbPath).parent_path());
      initDatabase(dbPath, schemaPath);
      std::cout << "DB initialized at: " << dbPath << "\n";
      return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "--serve") {
      mdm::run_http_server(envPortOrDefault());
      return 0;
    }

    print_usage(argv[0]);
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 2;
  }
}
