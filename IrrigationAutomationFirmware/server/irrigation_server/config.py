"""Environment-backed settings for the local irrigation server."""

from dataclasses import dataclass
import json
import os
from pathlib import Path


@dataclass(frozen=True)
class Settings:
    database_path: str
    bearer_token: str
    node: str = "node"
    validator_cli: str = ""
    validator_options: dict | None = None

    @classmethod
    def from_env(cls):
        root = Path(__file__).resolve().parents[2]
        fields = json.loads(os.getenv("IRRIGATION_FIELD_REGISTRY", '[{"field_id":"field-001","valve_id":"valve1","enabled":true,"flow_meter":"commissioned"}]'))
        allowed_valve_sets = json.loads(os.getenv("IRRIGATION_ALLOWED_VALVE_SETS", '[["valve1"]]'))
        options = {
            "clockSynchronized": os.getenv("IRRIGATION_CLOCK_SYNCHRONIZED", "yes").lower() == "yes",
            "gatewayMaxRuntimeSec": int(os.getenv("IRRIGATION_GATEWAY_MAX_RUNTIME_SEC", "3600")),
            "fields": fields,
            "allowedValveSets": allowed_valve_sets,
            "durationUnits": _csv("IRRIGATION_DURATION_UNITS", "seconds,minutes,hours"),
            "volumeUnits": _csv("IRRIGATION_VOLUME_UNITS", "m3,liters"),
        }
        return cls(
            database_path=os.getenv("IRRIGATION_DATABASE_PATH", str(root / "data" / "watering-plans.sqlite3")),
            bearer_token=os.getenv("IRRIGATION_PLAN_API_TOKEN", ""),
            node=os.getenv("IRRIGATION_NODE", "node"),
            validator_cli=os.getenv("IRRIGATION_VALIDATOR_CLI", str(root / "tools" / "watering_plan_contract_cli.js")),
            validator_options=options,
        )


def _csv(name, default):
    return [item.strip() for item in os.getenv(name, default).split(",") if item.strip()]
