#pragma once
#include <string>
#include <optional>

struct ObjectRecord {
  std::string id;
  std::string logical_name;
  std::string mission_id;
  std::string sensor;
  std::string platform;
  std::string classification;
  std::string tags_json;
  int64_t     bytes;
  std::string sha256;
  std::string storage_tier;
  std::string storage_path;
  int64_t     created_at;
  int64_t     updated_at;

  // extended fields used by uxv-secure-pipeline
  std::string object_type;
  std::string content_type;
  int64_t     capture_time;
  std::string pipeline_run_id;
};

class MetadataStore {
public:
  explicit MetadataStore(const std::string& dbPath);
  void insertObject(const ObjectRecord& r);
  void appendHistory(const std::string& object_id,
                     const std::string& event,
                     const std::string& details_json,
                     int64_t at,
                     const std::string& actor);
  // ...existing APIs...
private:
  void* db_; // sqlite3*
};
