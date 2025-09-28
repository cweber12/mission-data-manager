// src/core/metadata/InitDb.cpp
#include "InitDb.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

static void execAll(sqlite3* db, const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQLite exec failed: " + msg);
    }
}

bool initDatabase(const std::string& dbPath, const std::string& schemaPath) {
    std::filesystem::create_directories(std::filesystem::path(dbPath).parent_path());

    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(
        dbPath.c_str(),
        &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );
    if (rc != SQLITE_OK) throw std::runtime_error("Failed to open DB: " + std::string(sqlite3_errmsg(db)));

    try {
        // Pragmas: concurrency + durability + integrity
        execAll(db, "PRAGMA journal_mode=WAL;");
        execAll(db, "PRAGMA synchronous=NORMAL;");   // or FULL if you prefer stronger durability
        execAll(db, "PRAGMA foreign_keys=ON;");
        execAll(db, "PRAGMA busy_timeout=5000;");

        // Load schema file and apply (safe: CREATE TABLE IF NOT EXISTS ...)
        std::ifstream in(schemaPath);
        if (!in) throw std::runtime_error("Cannot open schema file: " + schemaPath);
        std::ostringstream buf; buf << in.rdbuf();
        execAll(db, buf.str());

        // Optional: track schema version
        execAll(db, "PRAGMA user_version=1;");

        sqlite3_close(db);
        return true;
    } catch (...) {
        sqlite3_close(db);
        throw;
    }
}