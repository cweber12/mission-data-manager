#pragma once
#include <string>
#include <string_view>

class LocalFSBackend {
public:
  LocalFSBackend(std::string hotRoot, std::string coldRoot)
    : hotRoot_(std::move(hotRoot)), coldRoot_(std::move(coldRoot)) {}

  // Writes bytes into HOT storage under mission_id/id; returns full path.
  std::string put(const std::string& mission_id,
                  const std::string& id,
                  std::string_view bytes);

private:
  std::string hotRoot_;
  std::string coldRoot_;
};
