#include "HttpServer.hpp"

#include <httplib.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <ctime>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include <sstream>

#include "core/metadata/MetadataStore.hpp"
#include "core/storage/LocalFSBackend.hpp"

// -------- SHA-256 (Windows CNG) --------
#ifdef _WIN32
  #include <windows.h>
  #include <bcrypt.h>
  #pragma comment(lib, "bcrypt")
#endif

using nlohmann::json;

static std::string to_hex(const uint8_t* data, size_t len) {
  static const char* k = "0123456789abcdef";
  std::string out; out.resize(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out[2*i]   = k[(data[i] >> 4) & 0xF];
    out[2*i+1] = k[data[i] & 0xF];
  }
  return out;
}

static std::string sha256_hex(std::string_view bytes) {
#ifdef _WIN32
  BCRYPT_ALG_HANDLE hAlg = nullptr;
  BCRYPT_HASH_HANDLE hHash = nullptr;
  NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (st < 0) throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
  DWORD objLen = 0, cb = 0, hashLen = 0;
  st = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0);
  if (st < 0) { BCryptCloseAlgorithmProvider(hAlg, 0); throw std::runtime_error("GetProp OBJ_LEN failed"); }
  st = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0);
  if (st < 0) { BCryptCloseAlgorithmProvider(hAlg, 0); throw std::runtime_error("GetProp HASH_LEN failed"); }
  std::vector<uint8_t> obj(objLen), hash(hashLen);
  st = BCryptCreateHash(hAlg, &hHash, obj.data(), objLen, nullptr, 0, 0);
  if (st < 0) { BCryptCloseAlgorithmProvider(hAlg, 0); throw std::runtime_error("CreateHash failed"); }
  if (!bytes.empty()) {
    st = BCryptHashData(hHash, (PUCHAR)bytes.data(), (ULONG)bytes.size(), 0);
    if (st < 0) { BCryptDestroyHash(hHash); BCryptCloseAlgorithmProvider(hAlg, 0); throw std::runtime_error("HashData failed"); }
  }
  st = BCryptFinishHash(hHash, hash.data(), (ULONG)hash.size(), 0);
  BCryptDestroyHash(hHash);
  BCryptCloseAlgorithmProvider(hAlg, 0);
  if (st < 0) throw std::runtime_error("FinishHash failed");
  return to_hex(hash.data(), hash.size());
#else
  // Fallback (non-Windows) — TODO: replace with OpenSSL or std::sha256 when available
  std::hash<std::string_view> h; // NOT cryptographic, placeholder only
  size_t v = h(bytes);
  std::ostringstream oss; oss << std::hex << v;
  auto s = oss.str();
  // pad to 64 hex chars to satisfy NOT NULL constraint
  if (s.size() < 64) s.append(64 - s.size(), '0');
  return s;
#endif
}

// -------- helpers --------

static std::string uuid4() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  auto rnd64 = [&]() { return static_cast<uint64_t>(rng()); };
  auto hexn = [](uint64_t v, int n) {
    static const char* k = "0123456789abcdef";
    std::string s(n, '0');
    for (int i = n - 1; i >= 0; --i) { s[i] = k[v & 0xf]; v >>= 4; }
    return s;
  };
  uint64_t a = rnd64(), b = rnd64();
  // version 4
  b = (b & 0xffffffffffff0fffULL) | 0x0000000000004000ULL;
  // variant 10xx...
  b = (b & 0x3fffffffffffffffULL) | 0x8000000000000000ULL;

  return hexn(a >> 32, 8) + "-" + hexn(a & 0xffffffffULL, 4) + "-" +
         hexn((b >> 48) & 0xffffULL, 4) + "-" + hexn((b >> 32) & 0xffffULL, 4) + "-" +
         hexn(b & 0xffffffffULL, 8) + hexn(rnd64() & 0xffffffffULL, 8);
}

static bool check_api_key(const httplib::Request& req,
                          const std::string& apiKey,
                          httplib::Response& res) {
  if (apiKey.empty()) return true; // auth disabled for now
  auto k = req.get_header_value("X-API-Key");
  if (k == apiKey) return true;
  res.status = 401;
  res.set_content("unauthorized", "text/plain");
  return false;
}

static std::string param_or(const httplib::Request& req, const char* k, const std::string& def = {}) {
  if (auto it = req.params.find(k); it != req.params.end()) return it->second;
#if defined(HTTPLIB_VERSION)
  try { return req.get_param_value(k); } catch (...) {}
#endif
  return def;
}

// -------- server --------

namespace mdm {

void run_http_server(MetadataStore& store,
                     LocalFSBackend& fs,
                     int port,
                     const std::string& apiKey) {
  httplib::Server svr;

  // Health check
  svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
    res.status = 200;
    res.set_content("ok", "text/plain");
  });

  // POST /ingest
  // Body: raw bytes of the file
  // Metadata: X-MDM-Meta: <JSON>   (or)  ?meta=<urlencoded JSON>   (fallback)
  // Also supports individual query params for quick tests.
  svr.Post("/ingest", [&](const httplib::Request& req, httplib::Response& res) {
    if (!check_api_key(req, apiKey, res)) return;

    const auto& bytes = req.body;
    if (bytes.empty()) {
      res.status = 400; res.set_content("empty body", "text/plain"); return;
    }

    std::string meta_json = req.get_header_value("X-MDM-Meta");
    if (meta_json.empty()) meta_json = param_or(req, "meta", "");

    json j = json::object();
    if (!meta_json.empty()) {
      try { j = json::parse(meta_json); }
      catch (...) { res.status = 400; res.set_content("invalid JSON in metadata", "text/plain"); return; }
    }

    // required
    std::string mission_id = j.contains("mission_id") ? j["mission_id"].get<std::string>()
                                                      : param_or(req, "mission_id");
    if (mission_id.empty()) {
      res.status = 422; res.set_content("metadata.mission_id required", "text/plain"); return;
    }

    // optional helpers
    auto get_s = [&](const char* k, const std::string& def = std::string()) {
      if (j.contains(k) && j[k].is_string()) return j[k].get<std::string>();
      return param_or(req, k, def);
    };
    auto get_i64 = [&](const char* k, int64_t def) {
      if (j.contains(k) && j[k].is_number_integer()) return j[k].get<int64_t>();
      auto s = param_or(req, k);
      if (!s.empty()) try { return std::stoll(s); } catch (...) {}
      return def;
    };
    auto get_json_obj = [&](const char* k) -> json {
      if (j.contains(k) && j[k].is_object()) return j[k];
      return json::object();
    };

    const std::string id              = get_s("id", uuid4());
    const std::string logical_name    = get_s("logical_name", "upload.bin");
    const std::string sensor          = get_s("sensor", "");
    const std::string platform        = get_s("platform", "");
    const std::string classification  = get_s("classification", "UNCLASS");
    const std::string object_type     = get_s("object_type", "");
    const std::string content_type    =
      (j.contains("content_type") && j["content_type"].is_string())
        ? j["content_type"].get<std::string>()
        : (req.get_header_value("Content-Type").empty()
             ? "application/octet-stream"
             : req.get_header_value("Content-Type"));
    const int64_t     capture_time    = get_i64("capture_time", static_cast<int64_t>(std::time(nullptr)));
    const std::string pipeline_run_id = get_s("pipeline_run_id", "");
    const json        tags            = get_json_obj("tags");

    const std::string sha256 = sha256_hex(std::string_view(bytes));
    const int64_t now = static_cast<int64_t>(std::time(nullptr));

    // Write to HOT storage and persist metadata
    const std::string storage_path = fs.put(mission_id, id, std::string_view(bytes));

    ObjectRecord rec {
      /*id*/             id,
      /*logical_name*/   logical_name,
      /*mission_id*/     mission_id,
      /*sensor*/         sensor,
      /*platform*/       platform,
      /*classification*/ classification,
      /*tags_json*/      tags.dump(),
      /*bytes*/          static_cast<int64_t>(bytes.size()),
      /*sha256*/         sha256,
      /*storage_tier*/   "HOT",
      /*storage_path*/   storage_path,
      /*created_at*/     now,
      /*updated_at*/     now,
      /*object_type*/    object_type,
      /*content_type*/   content_type,
      /*capture_time*/   capture_time,
      /*pipeline_run_id*/pipeline_run_id
    };

    try {
      store.insertObject(rec);
      store.appendHistory(id, "CREATED", json({{"source","/ingest"}}).dump(), now, "api");
    } catch (const std::exception& e) {
      spdlog::error("insert failed: {}", e.what());
      res.status = 500;
      res.set_content("insert failed", "text/plain");
      return;
    }

    json out = {
      {"id", id},
      {"sha256", sha256},
      {"storage_tier", "HOT"},
      {"storage_path", storage_path}
    };
    res.status = 200;
    res.set_content(out.dump(), "application/json");
  });

  // Fallback
  svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
    if (res.status == 404) res.set_content("not found", "text/plain");
  });

  spdlog::info("HTTP server listening on http://0.0.0.0:{}", port);
  if (!svr.listen("0.0.0.0", port)) {
    spdlog::error("Failed to bind port {}", port);
  }
}

} // namespace mdm
