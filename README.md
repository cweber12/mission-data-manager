# Mission Data Manager (MDM)

A C++ microservice that catalogs **mission artifacts** (logs/telemetry/media) with metadata in SQLite and lays the groundwork for policy-based lifecycle (HOT/WARM/COLD). It’s designed to integrate with `uxv-secure-pipeline` as an ingest/metadata service.

---

## Current Status (implemented)

### Build system

- **CMake** project with **vcpkg manifest** deps:
  - `sqlite3` (features: **json1**, **fts5**)
  - `spdlog`
  - `nlohmann-json`
  - `cpp-httplib`
- Windows/MSVC flags: `/utf-8`, `_WIN32_WINNT=0x0A00`, `WINVER=0x0A00`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, `_CRT_SECURE_NO_WARNINGS`.
- Separate generator build dirs: `build-vs` (Visual Studio) and `build-ninja` (Ninja).

### Database

- SQLite DB at **`data/mission-metadata.db`** (override with `MDM_DB_PATH`).
- Schema file **`src/core/metadata/schema.sql`** is **copied next to the binary** on build.
- One-time init via **`mdm --init`** (idempotent `CREATE TABLE IF NOT EXISTS ...`).
- JSON tagging supported (SQLite JSON1); FTS5 enabled for future text search.

### Runtime

- Minimal HTTP server with **`GET /health`**.
- Helper script **`scripts/build-run.ps1`** to configure, build, init DB, and (optionally) run the server.

### CI

- GitHub Actions workflow builds with `vcpkg` toolchain, runs `mdm --init`, and **verifies tables exist** using the `sqlite3` CLI.

---

## Quicks Start (Windows)

### Prerequisites

- Visual Studio 2022 Build Tools (Desktop C++ workload) or VS 2022 Community
- CMake
- vcpkg cloned and bootstrapped (e.g., `C:\dev\vcpkg`)

### Build, initialize DB, run

```powershell
# from repo root
.\scripts\build-run.ps1 -Generator vs -Clean -Init -Serve
# Server on http://localhost:8080
```

### Smoke test

```powershell
Invoke-WebRequest http://localhost:8080/health | Select-Object -Expand Content
# -> ok
```

## Configuration

### Environment variables

- MDM_DB_PATH — path to SQLite file (default data/mission-metadata.db)
- MDM_PORT — HTTP port for --serve (default 8080)
- (planned) MDM_API_KEY — required API key for mutating endpoints

### Example run

```powershell
$env:MDM_DB_PATH = "data\mission-metadata.db"
$env:MDM_PORT = "8080"
.\build-vs\Release\mdm.exe --serve
```

## Database Model (current)

### Tables (see `src/core/metadata/schema.sql`)

- objects — one row per stored file/blob, mission/meta fields, JSON tags, tier/path, checksum, timestamps.
- object_history — append-only event log for auditability (CREATED|MIGRATED|TAGGED|ACCESSED|DELETED).
- object_links — provenance links (e.g., pipeline step inputs/outputs).

### Key capabilities in code

- InitDb sets pragmas (WAL, foreign keys, busy_timeout) and executes the schema.
- MetadataStore (skeleton) encapsulates inserts/queries/history (to be expanded)
