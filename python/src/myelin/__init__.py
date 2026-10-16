from importlib import import_module
from importlib.metadata import PackageNotFoundError, version

from .runtime import (
    Buffer, Deployment, Device, Diagnostic, Entrypoint, Input, Metadata, MyeLiNError, Parameter,
    Requirements, Route, Runtime, Section, Session, Signature,
)

_QWEN_EXPORTS = {"QwenContent", "QwenImage", "QwenInput", "QwenText"}

try:
    __version__ = version("myelin-runtime")
except PackageNotFoundError:
    __version__ = "0+unknown"


def __getattr__(name: str):
    if name in _QWEN_EXPORTS:
        return getattr(import_module(".qwen3", __name__), name)
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")

__all__ = [
    "__version__", "Buffer", "Deployment", "Device", "Diagnostic", "Entrypoint", "Input", "Metadata", "MyeLiNError",
    "Parameter", "QwenContent", "QwenImage", "QwenInput", "QwenText", "Requirements", "Route",
    "Runtime", "Section", "Session", "Signature",
]
