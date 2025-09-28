#pragma once
namespace mdm {
  // Starts a blocking HTTP server with a simple /health endpoint.
  void run_http_server(int port);
}
