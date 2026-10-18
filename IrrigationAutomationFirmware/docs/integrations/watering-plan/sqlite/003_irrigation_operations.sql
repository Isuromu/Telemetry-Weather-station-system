BEGIN IMMEDIATE;

CREATE TABLE irrigation_operations (
    operation_id TEXT NOT NULL,
    action TEXT NOT NULL CHECK (action IN ('start', 'stop')),
    request_json TEXT NOT NULL,
    registered_at TEXT NOT NULL,
    PRIMARY KEY (operation_id, action)
);

CREATE INDEX irrigation_operations_registered_at_idx
    ON irrigation_operations (registered_at);

INSERT INTO schema_migrations(version, applied_at)
VALUES (3, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'));

COMMIT;
