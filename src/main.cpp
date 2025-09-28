// src/main.cpp
#include "core/metadata/InitDb.hpp"
#include <cstdlib>
#include <string>

static std::string defaultDbPath() {
  if (const char* p = std::getenv("MDM_DB_PATH")) return std::string(p);
  return "data/mission-metadata.db";  // <- your new default
}

int main(int argc, char** argv) {
  const std::string dbPath = defaultDbPath();
  const std::string schemaPath = "schema.sql"; // placed by configure_file()

  // one-shot init mode supported
  if (argc > 1 && std::string(argv[1]) == "--init") {
    initDatabase(dbPath, schemaPath);
    return 0;
  }

  initDatabase(dbPath, schemaPath);
  // start HTTP server, scheduler, etc.
}
