#pragma once
#include <string>

class MetadataStore;
class LocalFSBackend;

namespace mdm {
  // Start a blocking HTTP server with minimal endpoints.
  // apiKey: if empty, auth is disabled (useful for early integration).
  void run_http_server(MetadataStore& store,
                       LocalFSBackend& fs,
                       int port,
                       const std::string& apiKey);
}

