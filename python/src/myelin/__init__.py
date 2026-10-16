from importlib.metadata import PackageNotFoundError, version

from .runtime import (
    Buffer, Deployment, Device, Diagnostic, Entrypoint, Input, Metadata, MyeLiNError, Parameter,
    Requirements, Route, Runtime, Section, Session, Signature,
)

try:
    __version__ = version("myelin-runtime")
except PackageNotFoundError:
    __version__ = "0+unknown"
__all__ = [
    "__version__", "Buffer", "Deployment", "Device", "Diagnostic", "Entrypoint", "Input", "Metadata", "MyeLiNError",
    "Parameter", "Requirements", "Route",
    "Runtime", "Section", "Session", "Signature",
]
