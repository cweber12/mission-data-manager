#include "MetadataStore.hpp"
#include <stdexcept>
#include <sqlite3.h>

MetadataStore::MetadataStore(const std::string& dbPath) : db_(nullptr) {
  sqlite3* db=nullptr;
  if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr)!=SQLITE_OK) {
    throw std::runtime_error("failed to open db");
  }
  db_ = db;
}

void MetadataStore::insertObject(const ObjectRecord& r) {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql = R"SQL(
    INSERT INTO objects
      (id, logical_name, mission_id, sensor, platform, classification, tags, bytes, sha256,
       storage_tier, storage_path, created_at, updated_at,
       object_type, content_type, capture_time, pipeline_run_id)
    VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
  )SQL";
  sqlite3_stmt* st=nullptr;
  sqlite3_prepare_v2(db, sql, -1, &st, nullptr);
  int i=1;
  sqlite3_bind_text(st, i++, r.id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, i++, r.logical_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, i++, r.mission_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, i++, r.sensor.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, i++, r.platform.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, i++, r.classification.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, i++, r.tags_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, i++, r.bytes);
  sqlite3_bind_text(st, i++, r.sha256.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, i++, r.storage_tier.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, i++, r.storage_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, i++, r.created_at);
  sqlite3_bind_int64(st, i++, r.updated_at);
  sqlite3_bind_text(st, i++, r.object_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, i++, r.content_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, i++, r.capture_time);
  sqlite3_bind_text(st, i++, r.pipeline_run_id.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(st) != SQLITE_DONE) {
    std::string err = sqlite3_errmsg(db);
    sqlite3_finalize(st);
    throw std::runtime_error("insertObject failed: " + err);
  }
  sqlite3_finalize(st);
}

void MetadataStore::appendHistory(const std::string& object_id,
                                  const std::string& event,
                                  const std::string& details_json,
                                  int64_t at,
                                  const std::string& actor) {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql = R"SQL(
    INSERT INTO object_history (object_id, event, details, at, actor)
    VALUES (?,?,?,?,?)
  )SQL";
  sqlite3_stmt* st=nullptr;
  sqlite3_prepare_v2(db, sql, -1, &st, nullptr);
  sqlite3_bind_text(st, 1, object_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, event.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 3, details_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 4, at);
  sqlite3_bind_text(st, 5, actor.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(st) != SQLITE_DONE) {
    std::string err = sqlite3_errmsg(db);
    sqlite3_finalize(st);
    throw std::runtime_error("appendHistory failed: " + err);
  }
  sqlite3_finalize(st);
}
