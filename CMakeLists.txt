cmake_minimum_required(VERSION 3.22)
project(mission-data-manager LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(unofficial-sqlite3 CONFIG REQUIRED)     # keep this
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

add_library(mdm_core
  src/core/metadata/InitDb.cpp
  src/core/metadata/MetadataStore.cpp
  src/core/crypto/Hash.cpp
  src/core/storage/LocalFSBackend.cpp
)

target_link_libraries(mdm_core
  PRIVATE
    unofficial::sqlite3::sqlite3
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

add_executable(mdm
  src/main.cpp
  src/services/api/HttpServer.cpp
  src/services/scheduler/Scheduler.cpp
)
target_link_libraries(mdm PRIVATE mdm_core httplib::httplib)

configure_file(src/core/metadata/schema.sql schema.sql COPYONLY)
