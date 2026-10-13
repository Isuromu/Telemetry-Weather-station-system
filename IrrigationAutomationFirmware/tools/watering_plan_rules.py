"""Loader for the language-neutral watering-plan API rules.

`docs/integrations/watering-plan/api-protocol.rules.json` is the single source
for the transport and lifecycle rules (routes, error-code -> HTTP status, state
and transition tables, reason vocabularies). This module is the only place the
Python side turns an error code into a status, so the reference backend, the
fake backend and the conformance suite cannot drift from one another.
"""

from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path
from typing import Any, Dict, Optional

ROOT = Path(__file__).resolve().parents[1]
RULES_PATH = ROOT / "docs" / "integrations" / "watering-plan" / "api-protocol.rules.json"


@lru_cache(maxsize=4)
def load_rules(path: Optional[str] = None) -> Dict[str, Any]:
    target = Path(path) if path else RULES_PATH
    with open(str(target), "r", encoding="utf-8") as handle:
        return json.load(handle)


def error_status(path: Optional[str] = None) -> Dict[str, int]:
    return load_rules(path)["error_status"]


def status_for(code: Optional[str], default: Optional[int] = None, path: Optional[str] = None) -> Optional[int]:
    """HTTP status for an error code, per the rules table, else `default`."""
    aliases = load_rules(path).get("error_code_aliases", {})
    resolved = aliases.get(code, code)
    return error_status(path).get(resolved, default)


def reason_vocabulary(path: Optional[str] = None) -> Dict[str, list]:
    rules = load_rules(path)
    return {
        "delay_reasons": rules["delay_reasons"],
        "stop_reasons": rules["stop_reasons"],
        "failure_reasons": rules["failure_reasons"],
    }


__all__ = ["RULES_PATH", "load_rules", "error_status", "status_for", "reason_vocabulary"]
