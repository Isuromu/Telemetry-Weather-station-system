"""Typed settings for the local irrigation server."""

from dataclasses import dataclass


@dataclass(frozen=True)
class Settings:
    """Runtime settings supplied by the deployment environment."""

    database_path: str
    plan_api_token: str
