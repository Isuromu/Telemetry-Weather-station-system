-- Phase 3 result tracking. Keep migration 1 unchanged so deployed prototype
-- databases advance through the same explicit migration path as new ones.
BEGIN IMMEDIATE;

ALTER TABLE watering_attempts
    ADD COLUMN actual_runtime_ms INTEGER CHECK (actual_runtime_ms >= 0);

INSERT INTO schema_migrations(version, applied_at)
VALUES (2, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'));

COMMIT;
