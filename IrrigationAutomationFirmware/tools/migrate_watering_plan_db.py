#!/usr/bin/env python3
"""Apply the local watering-plan SQLite schema migrations."""

import argparse
import sqlite3
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MIGRATIONS = [
    (1, ROOT / "docs/integrations/watering-plan/sqlite/001_initial.sql"),
]
EXPECTED_TABLES = {
    "watering_plans",
    "watering_attempts",
    "watering_plan_events",
    "schema_migrations",
}


def table_names(connection):
    rows = connection.execute(
        "SELECT name FROM sqlite_master WHERE type = 'table'"
    )
    return {row[0] for row in rows}


def current_version(connection, tables):
    if "schema_migrations" not in tables:
        return 0
    row = connection.execute("SELECT MAX(version) FROM schema_migrations").fetchone()
    return row[0] or 0


def find_migration_file(version, expected_path):
    if expected_path.is_file():
        return expected_path
    if version == 1:
        script_directory = Path(__file__).resolve().parent
        for candidate in (
            script_directory / "001_initial.sql",
            script_directory / "sqlite" / "001_initial.sql",
        ):
            if candidate.is_file():
                return candidate
    raise FileNotFoundError(
        "Could not find {}. Put it beside this script or restore {}."
        .format(expected_path.name, expected_path)
    )


def migrate(database_path):
    database_path = Path(database_path).expanduser()
    database_path.parent.mkdir(parents=True, exist_ok=True)

    connection = sqlite3.connect(str(database_path), isolation_level=None)
    try:
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA journal_mode = WAL")
        connection.execute("PRAGMA synchronous = FULL")

        tables = table_names(connection)
        version = current_version(connection, tables)
        if version > MIGRATIONS[-1][0]:
            raise RuntimeError(
                "Database schema is newer than this firmware repository supports"
            )
        if version == 0 and tables.intersection(EXPECTED_TABLES - {"schema_migrations"}):
            raise RuntimeError(
                "Watering-plan tables exist without migration history; refusing to "
                "guess their schema"
            )

        applied = []
        for migration_version, migration_path in MIGRATIONS:
            if migration_version <= version:
                continue
            migration_path = find_migration_file(migration_version, migration_path)
            connection.executescript(migration_path.read_text(encoding="utf-8"))
            applied.append(migration_version)

        final_tables = table_names(connection)
        missing = EXPECTED_TABLES - final_tables
        if missing:
            raise RuntimeError(
                "Migration completed without required tables: "
                + ", ".join(sorted(missing))
            )
        return applied
    except Exception:
        if connection.in_transaction:
            connection.rollback()
        raise
    finally:
        connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "database",
        help="SQLite database file to initialize or migrate (for example, "
        "/var/lib/node-red/watering-plans.sqlite3)",
    )
    args = parser.parse_args()
    try:
        applied = migrate(args.database)
    except (OSError, sqlite3.Error, RuntimeError) as error:
        print("Watering-plan migration failed: {}".format(error), file=sys.stderr)
        return 1

    if applied:
        print("Applied watering-plan schema migration(s): {}".format(
            ", ".join(str(version) for version in applied)
        ))
    else:
        print("Watering-plan database schema is already current")
    return 0


if __name__ == "__main__":
    sys.exit(main())
