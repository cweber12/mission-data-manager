#include <httplib.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <random>
#include "HttpServer.hpp"
#include "core/metadata/MetadataStore.hpp"
#include "core/storage/LocalFSBackend.hpp"
#include "core/crypto/Hash.hpp"

using nlohmann::json;

namespace {

// simple UUIDv4 (good enough for IDs here)
std::string uuid4() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  auto gen64 = []() { return static_cast<uint64_t>(rng()); };
  auto to_hex = [](uint64_t v, int n) {
    static const char* k = "0123456789abcdef";
    std::string s(n,'0');
    for (int i=n-1; i>=0; --i) { s[i] = k[v & 0xf]; v >>= 4; }
    return s;
  };
  // 128 bits
  uint64_t a = gen64(), b = gen64();
  // set UUIDv4/version (bits 12-15 of time_hi_and_version)
  b = (b & 0xffffffffffff0fffULL) | 0x0000000000004000ULL;
  // set variant (10xxxxxx...)
  b = (b & 0x3fffffffffffffffULL) | 0x8000000000000000ULL;
  // format 8-4-4-4-12
  return to_hex(a >> 32, 8) + "-" + to_hex(a & 0xffffffffULL, 4) + "-" +
         to_hex((b >> 48) & 0xffffULL, 4) + "-" + to_hex((b >> 32) & 0xffffULL, 4) + "-" +
         to_hex(b & 0xffffffffULL, 8) + to_hex(gen64() & 0xffffffffULL, 8);
}

bool check_api_key(const httplib::Request& req, const std::string& apiKey, httplib::Response& res) {
  if (apiKey.empty()) return true; // auth disabled for early integration
  auto k = req.get_header_value("X-API-Key");
  if (k == apiKey) return true;
  res.status = 401;
  res.set_content("unauthorized", "text/plain");
  return false;
}

} // namespace

namespace mdm {

void run_http_server(MetadataStore& store, LocalFSBackend& fs, int port, const std::string& apiKey) {
  httplib::Server svr;

  // health
  svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
    res.status = 200; res.set_content("ok", "text/plain");
  });

  // ingest: multipart form with fields:
  //   - file            : the artifact bytes
  //   - metadata.json   : JSON blob describing the object
  svr.Post("/ingest", [&](const httplib::Request& req, httplib::Response& res) {
    if (!check_api_key(req, apiKey, res)) return;

    if (!req.is_multipart_form_data()) {
      res.status = 400; res.set_content("multipart/form-data required", "text/plain"); return;
    }
    auto fileField = req.get_file_value("file");
    auto metaField = req.get_file_value("metadata.json");
    if (fileField.content.empty() || metaField.content.empty()) {
      res.status = 400; res.set_content("missing file or metadata.json", "text/plain"); return;
    }

    json j;
    try { j = json::parse(metaField.content); }
    catch (...) { res.status = 400; res.set_content("invalid metadata.json", "text/plain"); return; }

    // Minimal required fields
    const auto mission_id   = j.value("mission_id", "");
    if (mission_id.empty()) { res.status = 422; res.set_content("metadata.mission_id required", "text/plain"); return; }

    // Assemble record
    std::string id = j.value("id", uuid4());
    std::string logical_name = j.value("logical_name", fileField.filename);
    std::string sensor       = j.value("sensor", "");
    std::string platform     = j.value("platform", "");
    std::string classification= j.value("classification", "UNCLASS");
    std::string object_type  = j.value("object_type", "");
    std::string content_type = j.value("content_type", "application/octet-stream");
    int64_t capture_time     = j.value("capture_time", static_cast<int64_t>(time(nullptr)));
    std::string pipeline_run_id = j.value("pipeline_run_id", "");
    json tags                = j.value("tags", json::object());

    const auto& bytes = fileField.content;
    std::string sha256 = Hash::sha256(bytes);
    int64_t now = static_cast<int64_t>(time(nullptr));

    // Write to HOT storage
    auto storage_path = fs.put(mission_id, id, bytes); // returns full path
    // Persist metadata
    store.insertObject({
      id, logical_name, mission_id, sensor, platform, classification,
      tags.dump(), static_cast<int64_t>(bytes.size()), sha256,
      "HOT", storage_path, now, now,
      object_type, content_type, capture_time, pipeline_run_id
    });
    store.appendHistory(id, "CREATED", json({{"source","/ingest"}}).dump(), now, "api");

    json out = {
      {"id", id},
      {"sha256", sha256},
      {"storage_tier", "HOT"},
      {"storage_path", storage_path}
    };
    res.set_content(out.dump(), "application/json");
  });

  spdlog::info("HTTP server listening on http://0.0.0.0:{}", port);
  if (!svr.listen("0.0.0.0", port)) {
    spdlog::error("Failed to bind port {}", port);
  }
}

} // namespace mdm

