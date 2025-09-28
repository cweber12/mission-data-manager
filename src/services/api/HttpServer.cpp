#include <httplib.h>
#include <spdlog/spdlog.h>
#include "HttpServer.hpp"

namespace mdm {

void run_http_server(int port) {
  httplib::Server svr;

  // Health check
  svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
    res.status = 200;
    res.set_content("ok", "text/plain");
  });

  // Basic 404
  svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
    if (res.status == 404) res.set_content("not found", "text/plain");
  });

  spdlog::info("HTTP server listening on http://0.0.0.0:{}", port);
  if (!svr.listen("0.0.0.0", port)) {
    spdlog::error("Failed to bind port {}", port);
  }
}

} // namespace mdm
