"""Local irrigation server package."""

from .app import create_app
from .config import Settings

__all__ = ["Settings", "create_app"]
