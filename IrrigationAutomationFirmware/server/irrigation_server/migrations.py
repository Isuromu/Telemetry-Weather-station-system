"""Apply the packaged watering-plan SQLite migrations."""
from pathlib import Path
import sqlite3

ROOT = Path(__file__).resolve().parents[1]
MIGRATIONS = [(1, ROOT / "migrations/001_initial.sql"), (2, ROOT / "migrations/002_attempt_tracking.sql")]
EXPECTED_TABLES = {"watering_plans", "watering_attempts", "watering_plan_events", "schema_migrations"}

def migrate(database_path):
    path = Path(database_path).expanduser()
    path.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(str(path), isolation_level=None)
    try:
        connection.execute("PRAGMA foreign_keys=ON")
        connection.execute("PRAGMA journal_mode=WAL")
        connection.execute("PRAGMA synchronous=FULL")
        tables = {r[0] for r in connection.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        version = connection.execute("SELECT MAX(version) FROM schema_migrations").fetchone()[0] or 0 if "schema_migrations" in tables else 0
        if version > MIGRATIONS[-1][0]:
            raise RuntimeError("Database schema is newer than this server supports")
        if version == 0 and tables.intersection(EXPECTED_TABLES - {"schema_migrations"}):
            raise RuntimeError("Watering-plan tables exist without migration history")
        applied = []
        for number, migration in MIGRATIONS:
            if number > version:
                connection.executescript(migration.read_text(encoding="utf-8"))
                applied.append(number)
        tables = {r[0] for r in connection.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        if EXPECTED_TABLES - tables:
            raise RuntimeError("Missing required tables: " + ", ".join(sorted(EXPECTED_TABLES - tables)))
        return applied
    finally:
        connection.close()
