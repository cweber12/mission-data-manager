-- Fresh install schema (recommended)
PRAGMA foreign_keys = ON;

-- objects = one row per stored file/blob
CREATE TABLE IF NOT EXISTS objects (
  id              TEXT PRIMARY KEY,              -- ulid/uuid
  logical_name    TEXT,                          -- filename or semantic name
  mission_id      TEXT NOT NULL,
  sensor          TEXT,
  platform        TEXT,
  classification  TEXT,                          

  -- NEW: explicit typing + content + capture timestamp + pipeline/run/source
  object_type     TEXT,                          -- log, telemetry, detection, video, manifest, etc
  content_type    TEXT,                          -- application/json, image/png, video/mp4, text/csv, etc
  capture_time    INTEGER,                       -- epoch when data was recorded (source time)
  pipeline_run_id TEXT,                          -- uxv-secure-pipeline execution
  source          TEXT,                          -- uxv-secure-pipeline input source (e.g. s3://bucket/key)

  tags            JSON,                          -- arbitrary KV (as JSON text)

  bytes           INTEGER NOT NULL,
  sha256          TEXT NOT NULL,
  storage_tier    TEXT NOT NULL CHECK (storage_tier IN ('HOT','WARM','COLD')),
  storage_path    TEXT NOT NULL,                 -- path or s3://bucket/key

  created_at      INTEGER NOT NULL,              -- epoch seconds (ingest)
  updated_at      INTEGER NOT NULL               -- epoch seconds (last metadata update)
);

-- history = append-only state transitions (for auditability)
CREATE TABLE IF NOT EXISTS object_history (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  object_id  TEXT NOT NULL,
  event      TEXT NOT NULL,                      -- CREATED|MIGRATED|TAGGED|ACCESSED|DELETED
  details    JSON,                               -- freeform
  at         INTEGER NOT NULL,
  actor      TEXT,                               -- api key, system, username
  FOREIGN KEY (object_id) REFERENCES objects(id) ON DELETE CASCADE
);

-- OPTIONAL: provenance/derivation links (parent -> child relations)
CREATE TABLE IF NOT EXISTS object_links (
  parent_id TEXT NOT NULL,
  relation  TEXT NOT NULL,                       -- 'derived-from' | 'uses' | 'annotates' | ...
  child_id  TEXT NOT NULL,
  PRIMARY KEY (parent_id, relation, child_id),
  FOREIGN KEY (parent_id) REFERENCES objects(id) ON DELETE CASCADE,
  FOREIGN KEY (child_id) REFERENCES objects(id) ON DELETE CASCADE
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_objects_mission        ON objects(mission_id);
CREATE INDEX IF NOT EXISTS idx_objects_tier           ON objects(storage_tier);
CREATE INDEX IF NOT EXISTS idx_objects_type           ON objects(object_type);
CREATE INDEX IF NOT EXISTS idx_objects_capture_time   ON objects(capture_time);
CREATE INDEX IF NOT EXISTS idx_objects_runid          ON objects(pipeline_run_id);
CREATE INDEX IF NOT EXISTS idx_objects_mission_time   ON objects(mission_id, capture_time);
CREATE INDEX IF NOT EXISTS idx_objects_mission_type_t ON objects(mission_id, object_type, capture_time);


