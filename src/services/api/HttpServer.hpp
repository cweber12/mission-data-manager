#pragma once
#include <string>

class MetadataStore;
class LocalFSBackend;

namespace mdm {
  // Starts a blocking HTTP server.
  // If apiKey is empty, auth is disabled (useful for early integration).
  void run_http_server(MetadataStore& store,
                       LocalFSBackend& fs,
                       int port,
                       const std::string& apiKey);
}



