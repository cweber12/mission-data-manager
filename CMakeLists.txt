cmake_minimum_required(VERSION 3.22)

project(mission-data-manager LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ----------------------------
# Dependencies via vcpkg
# ----------------------------
# Configure with:
#   -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
#   -DVCPKG_TARGET_TRIPLET=x64-windows
find_package(unofficial-sqlite3 CONFIG REQUIRED)      # target: unofficial::sqlite3::sqlite3
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

# ----------------------------
# Compiler flags / defines
# ----------------------------
if (MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
  add_compile_definitions(
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    _WIN32_WINNT=0x0A00  # Windows 10
    WINVER=0x0A00
  )
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# On Windows also link winsock (you already have this):
if (WIN32)
  target_link_libraries(mdm PRIVATE ws2_32)
endif()

# ----------------------------
# Core library
# ----------------------------
add_library(mdm_core
  src/core/metadata/InitDb.cpp
  src/core/metadata/MetadataStore.cpp
  src/core/crypto/Hash.cpp
  src/core/storage/LocalFSBackend.cpp
)

target_include_directories(mdm_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mdm_core
  PRIVATE
    unofficial::sqlite3::sqlite3
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# ----------------------------
# Executable
# ----------------------------
add_executable(mdm
  src/main.cpp
  src/services/api/HttpServer.cpp
  src/services/scheduler/Scheduler.cpp
)

target_link_libraries(mdm
  PRIVATE
    mdm_core
    httplib::httplib
)

# Windows sockets for cpp-httplib
if (WIN32)
  target_link_libraries(mdm PRIVATE ws2_32)
endif()

# ----------------------------
# Ship schema next to the exe
# ----------------------------
# Copy schema.sql to the runtime dir (Debug/Release) after building mdm
add_custom_command(TARGET mdm POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/src/core/metadata/schema.sql
          $<TARGET_FILE_DIR:mdm>/schema.sql
  COMMENT "Copying schema.sql to runtime directory"
)

# ----------------------------
# Initialize DB on build (idempotent)
# ----------------------------
# Set ON to create/update tables during build automatically.
option(MDM_INIT_DB_ON_BUILD "Initialize SQLite DB during build" ON)

if (MDM_INIT_DB_ON_BUILD)
  # Runs mdm --init with MDM_DB_PATH pointing to <repo>/data/mission-metadata.db
  add_custom_target(initdb ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/data
    COMMAND ${CMAKE_COMMAND} -E env
            "MDM_DB_PATH=${CMAKE_SOURCE_DIR}/data/mission-metadata.db"
            $<TARGET_FILE:mdm> --init
    WORKING_DIRECTORY $<TARGET_FILE_DIR:mdm>     # where schema.sql was copied
    DEPENDS mdm
    COMMENT "Initializing SQLite schema via mdm --init"
    VERBATIM
  )
endif()

# ----------------------------
# Optional: init via sqlite3 CLI (one-off helper)
# ----------------------------
# If you have the sqlite3 CLI installed, this provides:
#   cmake --build build --config Release --target initdb_cli
find_program(SQLITE3_EXE sqlite3)
if (SQLITE3_EXE)
  add_custom_target(initdb_cli
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/data
    COMMAND ${SQLITE3_EXE}
            ${CMAKE_SOURCE_DIR}/data/mission-metadata.db
            ".read ${CMAKE_CURRENT_SOURCE_DIR}/src/core/metadata/schema.sql"
    COMMENT "Initializing SQLite schema via sqlite3 CLI"
    VERBATIM
  )
endif()

# ----------------------------
# (Optional) Install targets
# ----------------------------
install(TARGETS mdm RUNTIME DESTINATION bin)
install(FILES src/core/metadata/schema.sql DESTINATION bin)

