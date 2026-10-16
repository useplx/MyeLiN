from __future__ import annotations

import ctypes
import ctypes.util
import os
import queue
import sys
import threading
import weakref
import warnings
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterator, Literal


_OK = 0
_CALLBACK_ERROR = 4


class _Diagnostic(ctypes.Structure):
    _fields_ = [
        ("domain", ctypes.c_char_p),
        ("code", ctypes.c_uint16),
        ("message", ctypes.c_char_p),
        ("location", ctypes.c_char_p),
        ("severity", ctypes.c_uint8),
    ]


class _Device(ctypes.Structure):
    _fields_ = [
        ("driver", ctypes.c_char_p),
        ("path", ctypes.c_char_p),
        ("name", ctypes.c_char_p),
    ]


class _Route(ctypes.Structure):
    _fields_ = [
        ("device", ctypes.c_char_p),
        ("invocations", ctypes.c_uint64),
        ("latency_ms", ctypes.c_double),
    ]


class _Section(ctypes.Structure):
    _fields_ = [("type", ctypes.c_uint32), ("name", ctypes.c_char_p),
                ("bytes", ctypes.c_uint64), ("optional", ctypes.c_uint8)]


class _Entrypoint(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char_p), ("runtime", ctypes.c_char_p),
                ("module", ctypes.c_char_p),
                ("symbol", ctypes.c_char_p)]


class _Input(ctypes.Structure):
    _fields_ = [("dtype", ctypes.c_char_p), ("shape", ctypes.POINTER(ctypes.c_int64)),
                ("rank", ctypes.c_size_t)]


class _Signature(ctypes.Structure):
    _fields_ = [("path", ctypes.c_char_p), ("module", ctypes.c_char_p),
                ("symbol", ctypes.c_char_p), ("inputs", ctypes.POINTER(_Input)),
                ("input_count", ctypes.c_size_t)]


class _Deployment(ctypes.Structure):
    _fields_ = [("path", ctypes.c_char_p), ("device", ctypes.c_uint32)]


class _Parameter(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char_p), ("codec", ctypes.c_char_p),
                ("storage", ctypes.c_char_p), ("bytes", ctypes.c_uint64)]


class _Requirements(ctypes.Structure):
    _fields_ = [("runtime_abi", ctypes.c_uint32), ("native_abi", ctypes.c_uint32),
                ("driver_abi", ctypes.c_uint32), ("platform", ctypes.c_char_p)]


@dataclass(frozen=True)
class Diagnostic:
    domain: str
    code: int
    message: str
    location: str = ""
    severity: str = "error"

    @property
    def identifier(self) -> str:
        code = f"E{self.code}" if self.code else "E0"
        return f"{self.domain}:{code}" if self.domain else code


@dataclass(frozen=True)
class Device:
    driver: str
    path: str
    name: str


@dataclass(frozen=True)
class Route:
    device: str
    invocations: int
    latency_ms: float


@dataclass(frozen=True)
class Section:
    type: int
    name: str
    bytes: int
    optional: bool


@dataclass(frozen=True)
class Entrypoint:
    name: str
    runtime: str
    module: str
    symbol: str


@dataclass(frozen=True)
class Input:
    dtype: str
    shape: tuple[int, ...]


@dataclass(frozen=True)
class Signature:
    path: str
    module: str
    symbol: str
    inputs: tuple[Input, ...]


@dataclass(frozen=True)
class Deployment:
    path: str
    device: int


@dataclass(frozen=True)
class Parameter:
    name: str
    codec: str
    storage: str
    bytes: int


@dataclass(frozen=True)
class Requirements:
    runtime_abi: int
    native_abi: int
    driver_abi: int
    platform: str


@dataclass(frozen=True)
class Metadata:
    backends: tuple[str, ...]
    sections: tuple[Section, ...] = ()
    entrypoints: tuple[Entrypoint, ...] = ()
    hooks: tuple[Entrypoint, ...] = ()
    signatures: tuple[Signature, ...] = ()
    deployment: tuple[Deployment, ...] = ()
    parameters: tuple[Parameter, ...] = ()
    requirements: Requirements = Requirements(0, 0, 0, "")

    @classmethod
    def read(cls, artifact: str | os.PathLike[str]) -> Metadata:
        path = os.fsencode(Path(artifact))
        if not path:
            raise ValueError("artifact path must not be empty")
        api = _api()
        found: list[str] = []

        def receive(value: bytes, _: ctypes.c_void_p) -> int:
            found.append((value or b"").decode(errors="replace"))
            return 0

        callback = api.StringCallback(receive)
        api.check(api.library.mln_artifact_enumerate_backends(path, callback, None))
        sections: list[Section] = []
        entries: list[Entrypoint] = []
        hooks: list[Entrypoint] = []
        signatures: list[Signature] = []
        deployment: list[Deployment] = []
        parameters: list[Parameter] = []

        def section(pointer: ctypes.POINTER(_Section), _: ctypes.c_void_p) -> int:
            value = pointer.contents
            sections.append(Section(value.type, (value.name or b"").decode(), value.bytes,
                                    bool(value.optional)))
            return 0

        def entry(target: list[Entrypoint]):
            def receive(pointer: ctypes.POINTER(_Entrypoint), _: ctypes.c_void_p) -> int:
                value = pointer.contents
                target.append(Entrypoint((value.name or b"").decode(),
                                          (value.runtime or b"").decode(),
                                          (value.module or b"").decode(),
                                          (value.symbol or b"").decode()))
                return 0
            return api.EntrypointCallback(receive)

        def deployed(pointer: ctypes.POINTER(_Deployment), _: ctypes.c_void_p) -> int:
            value = pointer.contents
            deployment.append(Deployment((value.path or b"").decode(), value.device))
            return 0

        def signature(pointer: ctypes.POINTER(_Signature), _: ctypes.c_void_p) -> int:
            value = pointer.contents
            inputs = tuple(
                Input((value.inputs[index].dtype or b"").decode(),
                      tuple(value.inputs[index].shape[dimension]
                            for dimension in range(value.inputs[index].rank)))
                for index in range(value.input_count)
            )
            signatures.append(Signature((value.path or b"").decode(),
                                        (value.module or b"").decode(),
                                        (value.symbol or b"").decode(), inputs))
            return 0

        def parameter(pointer: ctypes.POINTER(_Parameter), _: ctypes.c_void_p) -> int:
            value = pointer.contents
            parameters.append(Parameter((value.name or b"").decode(),
                                        (value.codec or b"").decode(),
                                        (value.storage or b"").decode(), value.bytes))
            return 0

        calls = (
            (api.library.mln_artifact_enumerate_sections, api.SectionCallback(section)),
            (api.library.mln_artifact_enumerate_entrypoints, entry(entries)),
            (api.library.mln_artifact_enumerate_hooks, entry(hooks)),
            (api.library.mln_artifact_enumerate_signatures,
             api.SignatureCallback(signature)),
            (api.library.mln_artifact_enumerate_deployment,
             api.DeploymentCallback(deployed)),
            (api.library.mln_artifact_enumerate_parameters,
             api.ParameterCallback(parameter)),
        )
        for function, receiver in calls:
            api.check(function(path, receiver, None))
        requirement = _Requirements()
        api.check(api.library.mln_artifact_requirements(path, ctypes.byref(requirement)))
        return cls(tuple(found), tuple(sections), tuple(entries), tuple(hooks),
                   tuple(signatures), tuple(deployment), tuple(parameters),
                   Requirements(requirement.runtime_abi, requirement.native_abi,
                                requirement.driver_abi,
                                (requirement.platform or b"").decode()))


class MyeLiNError(RuntimeError):
    def __init__(self, diagnostic: Diagnostic):
        self.diagnostic = diagnostic
        super().__init__(f"{diagnostic.identifier}: {diagnostic.message}")


def _library_path() -> str:
    explicit = os.environ.get("MLN_RUNTIME_LIBRARY")
    if explicit:
        return explicit
    discovered = ctypes.util.find_library("mln_runtime")
    if discovered:
        return discovered
    if os.name == "nt":
        roots = [Path(sys.executable).resolve().parent]
        bundle = getattr(sys, "_MEIPASS", None)
        if bundle:
            roots.insert(0, Path(bundle))
        for root in roots:
            for name in ("mln_runtime.dll", "libmln_runtime.dll"):
                candidate = root / name
                if candidate.is_file():
                    return str(candidate)
    raise RuntimeError(
        "libmln_runtime was not found; set MLN_RUNTIME_LIBRARY to its full path"
    )


class _Api:
    Callback = ctypes.CFUNCTYPE(
        ctypes.c_int, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.c_void_p
    )
    DiagnosticCallback = ctypes.CFUNCTYPE(
        ctypes.c_int, ctypes.POINTER(_Diagnostic), ctypes.c_void_p
    )
    DeviceCallback = ctypes.CFUNCTYPE(
        ctypes.c_int, ctypes.POINTER(_Device), ctypes.c_void_p
    )
    StringCallback = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p)
    RouteCallback = ctypes.CFUNCTYPE(
        ctypes.c_int, ctypes.POINTER(_Route), ctypes.c_void_p
    )
    SectionCallback = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.POINTER(_Section), ctypes.c_void_p)
    EntrypointCallback = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.POINTER(_Entrypoint), ctypes.c_void_p)
    SignatureCallback = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.POINTER(_Signature), ctypes.c_void_p)
    DeploymentCallback = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.POINTER(_Deployment), ctypes.c_void_p)
    ParameterCallback = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.POINTER(_Parameter), ctypes.c_void_p)
    BufferCallback = ctypes.CFUNCTYPE(
        ctypes.c_int, ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t, ctypes.c_void_p
    )

    def __init__(self) -> None:



        self.library = ctypes.CDLL(
            _library_path(), mode=getattr(ctypes, "RTLD_GLOBAL", ctypes.DEFAULT_MODE)
        )
        self.library.mln_c_api_version.restype = ctypes.c_uint32
        if self.library.mln_c_api_version() != 1:
            raise RuntimeError("unsupported MyeLiN C API version")

        self.library.mln_last_diagnostic.restype = ctypes.POINTER(_Diagnostic)
        self.library.mln_runtime_create.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
        self.library.mln_runtime_create.restype = ctypes.c_int
        self.library.mln_runtime_create_with_config.argtypes = [
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_void_p),
        ]
        self.library.mln_runtime_create_with_config.restype = ctypes.c_int
        self.library.mln_runtime_destroy.argtypes = [ctypes.c_void_p]
        self.library.mln_buffer_create.argtypes = [ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_int64), ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p)]
        self.library.mln_buffer_create.restype = ctypes.c_int
        self.library.mln_buffer_clone.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
        self.library.mln_buffer_clone.restype = ctypes.c_int
        self.library.mln_buffer_destroy.argtypes = [ctypes.c_void_p]
        self.library.mln_buffer_size.argtypes = [ctypes.c_void_p]
        self.library.mln_buffer_size.restype = ctypes.c_size_t
        self.library.mln_buffer_rank.argtypes = [ctypes.c_void_p]
        self.library.mln_buffer_rank.restype = ctypes.c_size_t
        self.library.mln_buffer_dimension.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        self.library.mln_buffer_dimension.restype = ctypes.c_int64
        self.library.mln_buffer_read.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
        self.library.mln_buffer_read.restype = ctypes.c_int
        self.library.mln_buffer_write.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
        self.library.mln_buffer_write.restype = ctypes.c_int
        self.library.mln_runtime_enumerate_devices.argtypes = [
            ctypes.c_void_p,
            self.DeviceCallback,
            ctypes.c_void_p,
        ]
        self.library.mln_runtime_enumerate_devices.restype = ctypes.c_int
        self.library.mln_artifact_enumerate_backends.argtypes = [
            ctypes.c_char_p,
            self.StringCallback,
            ctypes.c_void_p,
        ]
        self.library.mln_artifact_enumerate_backends.restype = ctypes.c_int
        for name, callback in (
            ("mln_artifact_enumerate_sections", self.SectionCallback),
            ("mln_artifact_enumerate_entrypoints", self.EntrypointCallback),
            ("mln_artifact_enumerate_hooks", self.EntrypointCallback),
            ("mln_artifact_enumerate_signatures", self.SignatureCallback),
            ("mln_artifact_enumerate_deployment", self.DeploymentCallback),
            ("mln_artifact_enumerate_parameters", self.ParameterCallback),
        ):
            function = getattr(self.library, name)
            function.argtypes = [ctypes.c_char_p, callback, ctypes.c_void_p]
            function.restype = ctypes.c_int
        self.library.mln_artifact_requirements.argtypes = [
            ctypes.c_char_p, ctypes.POINTER(_Requirements)]
        self.library.mln_artifact_requirements.restype = ctypes.c_int
        self.library.mln_session_create.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_void_p),
        ]
        self.library.mln_session_create.restype = ctypes.c_int
        self.library.mln_session_destroy.argtypes = [ctypes.c_void_p]
        self.library.mln_session_load.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self.library.mln_session_load.restype = ctypes.c_int
        self.library.mln_session_load_backend.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
        ]
        self.library.mln_session_load_backend.restype = ctypes.c_int
        self.library.mln_session_enumerate_devices.argtypes = [
            ctypes.c_void_p,
            self.StringCallback,
            ctypes.c_void_p,
        ]
        self.library.mln_session_enumerate_devices.restype = ctypes.c_int
        self.library.mln_session_enumerate_routes.argtypes = [
            ctypes.c_void_p,
            self.RouteCallback,
            ctypes.c_void_p,
        ]
        self.library.mln_session_enumerate_routes.restype = ctypes.c_int
        self.library.mln_session_set_diagnostic_callback.argtypes = [
            ctypes.c_void_p,
            self.DiagnosticCallback,
            ctypes.c_void_p,
        ]
        self.library.mln_session_set_diagnostic_callback.restype = ctypes.c_int
        self.library.mln_session_cancel.argtypes = [ctypes.c_void_p]
        self.library.mln_session_invoke.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            self.Callback,
            ctypes.c_void_p,
        ]
        self.library.mln_session_invoke.restype = ctypes.c_int
        self.library.mln_session_invoke_buffers.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t, self.BufferCallback, ctypes.c_void_p]
        self.library.mln_session_invoke_buffers.restype = ctypes.c_int
        self.library.mln_session_state.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
            ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p)]
        self.library.mln_session_state.restype = ctypes.c_int
        self.library.mln_session_set_state.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
            ctypes.c_void_p]
        self.library.mln_session_set_state.restype = ctypes.c_int
        self.library.mln_session_asset.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_void_p)]
        self.library.mln_session_asset.restype = ctypes.c_int

    def check(self, status: int) -> None:
        if status == _OK:
            return
        pointer = self.library.mln_last_diagnostic()
        if pointer:
            value = pointer.contents
            diagnostic = Diagnostic(
                (value.domain or b"").decode(errors="replace"),
                value.code,
                (value.message or b"runtime failure").decode(errors="replace"),
                (value.location or b"").decode(errors="replace"),
                "warning" if value.severity == 0 else "error",
            )
        else:
            diagnostic = Diagnostic("myelin", 0, f"runtime status {status}")
        raise MyeLiNError(diagnostic)


_api_lock = threading.Lock()
_api_instance: _Api | None = None


def _api() -> _Api:
    global _api_instance
    with _api_lock:
        if _api_instance is None:
            _api_instance = _Api()
    return _api_instance


class Buffer:
    def __init__(self, size: int, shape: tuple[int, ...] = ()) -> None:
        if size < 0 or any(dimension <= 0 for dimension in shape):
            raise ValueError("buffer size and dimensions must be valid")
        self._api = _api()
        self._handle = ctypes.c_void_p()
        dimensions = (ctypes.c_int64 * len(shape))(*shape)
        self._api.check(self._api.library.mln_buffer_create(
            size, dimensions if shape else None, len(shape), ctypes.byref(self._handle)))

    @classmethod
    def _adopt(cls, handle: ctypes.c_void_p) -> Buffer:
        value = cls.__new__(cls)
        value._api = _api()
        value._handle = handle
        return value

    @classmethod
    def from_bytes(cls, value: bytes | bytearray | memoryview,
                   shape: tuple[int, ...] = ()) -> Buffer:
        data = bytes(value)
        buffer = cls(len(data), shape)
        buffer.write(data)
        return buffer

    @property
    def shape(self) -> tuple[int, ...]:
        rank = self._api.library.mln_buffer_rank(self._handle)
        return tuple(self._api.library.mln_buffer_dimension(self._handle, index)
                     for index in range(rank))

    def __len__(self) -> int:
        return self._api.library.mln_buffer_size(self._handle)

    def read(self, offset: int = 0, size: int | None = None) -> bytes:
        length = len(self) - offset if size is None else size
        if offset < 0 or length < 0:
            raise ValueError("buffer range must be valid")
        output = (ctypes.c_uint8 * length)()
        self._api.check(self._api.library.mln_buffer_read(
            self._handle, offset, output, length))
        return bytes(output)

    def write(self, value: bytes | bytearray | memoryview, offset: int = 0) -> None:
        data = bytes(value)
        source = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
        self._api.check(self._api.library.mln_buffer_write(
            self._handle, offset, source, len(data)))

    def close(self) -> None:
        if self._handle:
            self._api.library.mln_buffer_destroy(self._handle)
            self._handle = ctypes.c_void_p()

    def __bytes__(self) -> bytes:
        return self.read()

    def __del__(self) -> None:
        if hasattr(self, "_handle"):
            self.close()


class Runtime:
    def __init__(self, configuration: str | os.PathLike[str] | None = None) -> None:
        self._api = _api()
        self._handle = ctypes.c_void_p()
        self._lock = threading.RLock()
        self._condition = threading.Condition(self._lock)
        self._closing = False
        self._sessions: weakref.WeakSet[Session] = weakref.WeakSet()
        if configuration is None:
            status = self._api.library.mln_runtime_create(ctypes.byref(self._handle))
        else:
            path = os.fsencode(Path(configuration))
            if not path:
                raise ValueError("configuration path must not be empty")
            status = self._api.library.mln_runtime_create_with_config(
                path, ctypes.byref(self._handle)
            )
        self._api.check(status)

    def session(self) -> Session:
        with self._lock:
            if not self._handle or self._closing:
                raise RuntimeError("runtime is closed")
            session = Session(self)
            self._sessions.add(session)
        return session

    def devices(self) -> list[Device]:
        found: list[Device] = []

        def receive(pointer: ctypes.POINTER(_Device), _: ctypes.c_void_p) -> int:
            value = pointer.contents
            found.append(Device(
                (value.driver or b"").decode(errors="replace"),
                (value.path or b"").decode(errors="replace"),
                (value.name or b"").decode(errors="replace"),
            ))
            return 0

        callback = self._api.DeviceCallback(receive)
        with self._lock:
            if not self._handle or self._closing:
                raise RuntimeError("runtime is closed")
            self._api.check(self._api.library.mln_runtime_enumerate_devices(
                self._handle, callback, None
            ))
        return found

    def close(self) -> None:
        with self._lock:
            sessions = list(self._sessions)
            for session in sessions:
                with session._lock:
                    if (threading.current_thread() in session._workers or
                            getattr(session._callbacks, "active", False)):
                        raise RuntimeError("cannot close runtime from an invocation callback")
            while self._closing:
                self._condition.wait()
            if not self._handle:
                return
            self._closing = True
        try:
            for session in sessions:
                session.close()
            with self._lock:
                self._api.library.mln_runtime_destroy(self._handle)
                self._handle = ctypes.c_void_p()
        finally:
            with self._lock:
                self._closing = False
                self._condition.notify_all()

    def __enter__(self) -> Runtime:
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def __del__(self) -> None:
        if hasattr(self, "_handle"):
            self.close()


class Session:
    def __init__(self, runtime: Runtime) -> None:
        self._runtime = runtime
        self._api = runtime._api
        self._handle = ctypes.c_void_p()
        self._lock = threading.RLock()
        self._condition = threading.Condition(self._lock)
        self._closing = False
        self._callbacks = threading.local()
        self._diagnostic_handler: (
            Callable[[Diagnostic], Literal["continue", "ignore", "promote"]] | None
        ) = None
        self._diagnostic_callback = self._api.DiagnosticCallback(
            self._report_diagnostic
        )
        self._workers: set[threading.Thread] = set()
        with runtime._lock:
            if not runtime._handle or runtime._closing:
                raise RuntimeError("runtime is closed")
            self._api.check(
                self._api.library.mln_session_create(
                    runtime._handle, ctypes.byref(self._handle)
                )
            )
            self._api.check(
                self._api.library.mln_session_set_diagnostic_callback(
                    self._handle, self._diagnostic_callback, None
                )
            )
            runtime._sessions.add(self)

    def _report_diagnostic(self, pointer: ctypes.POINTER(_Diagnostic),
                           _: ctypes.c_void_p) -> int:
        active = getattr(self._callbacks, "active", False)
        self._callbacks.active = True
        try:
            return self._diagnostic(pointer)
        finally:
            self._callbacks.active = active

    def _diagnostic(self, pointer: ctypes.POINTER(_Diagnostic)) -> int:
        value = pointer.contents
        diagnostic = Diagnostic(
            (value.domain or b"").decode(errors="replace"),
            value.code,
            (value.message or b"").decode(errors="replace"),
            (value.location or b"").decode(errors="replace"),
            "warning" if value.severity == 0 else "error",
        )
        if self._diagnostic_handler is not None:
            action = self._diagnostic_handler(diagnostic)
            return {"continue": 0, "ignore": 1, "promote": 2}.get(action, 0)
        suffix = f" [{diagnostic.location}]" if diagnostic.location else ""
        warnings.warn(
            f"{diagnostic.identifier}: {diagnostic.message}{suffix}",
            RuntimeWarning,
            stacklevel=2,
        )
        return 0

    def set_diagnostic_handler(
        self,
        handler: Callable[[Diagnostic], Literal["continue", "ignore", "promote"]] | None,
    ) -> None:
        """Select how package warnings affect subsequent load/invoke calls."""
        with self._lock:
            self._require_open()
            self._diagnostic_handler = handler

    def load(self, artifact: str | os.PathLike[str], backend: str | None = None) -> None:
        if not os.fspath(artifact):
            raise ValueError("artifact path must not be empty")
        if backend is not None and not backend:
            raise ValueError("backend must not be empty")
        path = os.fsencode(Path(artifact))
        with self._lock:
            self._require_open()
            if backend is None:
                status = self._api.library.mln_session_load(self._handle, path)
            else:
                status = self._api.library.mln_session_load_backend(
                    self._handle, path, backend.encode()
                )
            self._api.check(status)

    def devices(self) -> list[str]:
        found: list[str] = []

        def receive(value: bytes, _: ctypes.c_void_p) -> int:
            found.append((value or b"").decode(errors="replace"))
            return 0

        callback = self._api.StringCallback(receive)
        with self._lock:
            self._require_open()
            self._api.check(self._api.library.mln_session_enumerate_devices(
                self._handle, callback, None
            ))
        return found

    def routes(self) -> list[Route]:
        found: list[Route] = []

        def receive(value: ctypes.POINTER(_Route), _: ctypes.c_void_p) -> int:
            route = value.contents
            found.append(Route(
                device=(route.device or b"").decode(errors="replace"),
                invocations=int(route.invocations),
                latency_ms=float(route.latency_ms),
            ))
            return 0

        callback = self._api.RouteCallback(receive)
        with self._lock:
            self._require_open()
            self._api.check(self._api.library.mln_session_enumerate_routes(
                self._handle, callback, None
            ))
        return found

    def invoke(self, value: bytes | bytearray | memoryview | str,
               entrypoint: str | None = None) -> bytes:
        return b"".join(self.stream(value, entrypoint))

    def buffers(self, entrypoint: str, inputs: list[Buffer] | tuple[Buffer, ...]) -> list[Buffer]:
        handles = (ctypes.c_void_p * len(inputs))(*(item._handle for item in inputs))
        outputs: list[Buffer] = []

        def receive(values: ctypes.POINTER(ctypes.c_void_p), count: int,
                    _: ctypes.c_void_p) -> int:
            for index in range(count):
                clone = ctypes.c_void_p()
                self._api.check(self._api.library.mln_buffer_clone(
                    values[index], ctypes.byref(clone)))
                outputs.append(Buffer._adopt(clone))
            return 0

        callback = self._api.BufferCallback(receive)
        with self._lock:
            self._require_open()
            self._api.check(self._api.library.mln_session_invoke_buffers(
                self._handle, entrypoint.encode(), handles, len(inputs), callback, None))
        return outputs

    def state(self, name: str, value: int | Buffer) -> Buffer:
        with self._lock:
            self._require_open()
            if isinstance(value, Buffer):
                self._api.check(self._api.library.mln_session_set_state(
                    self._handle, name.encode(), value._handle))
                size = len(value)
            else:
                size = value
            output = ctypes.c_void_p()
            self._api.check(self._api.library.mln_session_state(
                self._handle, name.encode(), size, ctypes.byref(output)))
            return Buffer._adopt(output)

    def asset(self, name: str) -> Buffer:
        output = ctypes.c_void_p()
        with self._lock:
            self._require_open()
            self._api.check(self._api.library.mln_session_asset(
                self._handle, name.encode(), ctypes.byref(output)))
        return Buffer._adopt(output)

    def stream(self, value: bytes | bytearray | memoryview | str,
               entrypoint: str | None = None) -> Iterator[bytes]:
        payload = value.encode() if isinstance(value, str) else bytes(value)
        chunks: queue.Queue[bytes | BaseException | None] = queue.Queue()
        cancelled = threading.Event()

        def receive(data: ctypes.POINTER(ctypes.c_uint8), size: int,
                    _: ctypes.c_void_p) -> int:
            if cancelled.is_set():
                return 1
            chunks.put(ctypes.string_at(data, size))
            return 0

        callback = self._api.Callback(receive)

        def execute() -> None:
            try:
                array = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)
                pointer = ctypes.cast(array, ctypes.POINTER(ctypes.c_uint8))
                encoded_entrypoint = entrypoint.encode() if entrypoint else None
                with self._lock:
                    self._require_open()
                    handle = ctypes.c_void_p(self._handle.value)
                status = self._api.library.mln_session_invoke(
                    handle,
                    encoded_entrypoint,
                    pointer,
                    len(payload),
                    callback,
                    None,
                )
                if status != _CALLBACK_ERROR or not cancelled.is_set():
                    self._api.check(status)
            except BaseException as error:
                chunks.put(error)
            finally:
                chunks.put(None)

        worker = threading.Thread(target=execute, name="myelin-invoke", daemon=True)
        with self._lock:
            self._require_open()
            self._workers.add(worker)
            try:
                worker.start()
            except BaseException:
                self._workers.discard(worker)
                raise
        try:
            while True:
                chunk = chunks.get()
                if chunk is None:
                    break
                if isinstance(chunk, BaseException):
                    raise chunk
                yield chunk
        finally:
            cancelled.set()
            with self._lock:
                if self._handle:
                    self._api.library.mln_session_cancel(self._handle)
            worker.join()
            with self._lock:
                self._workers.discard(worker)

    def close(self) -> None:
        workers: list[threading.Thread]
        with self._lock:
            if (threading.current_thread() in self._workers or
                    getattr(self._callbacks, "active", False)):
                raise RuntimeError("cannot close session from an invocation callback")
            while self._closing:
                self._condition.wait()
            if not self._handle:
                return
            self._closing = True
            self._api.library.mln_session_cancel(self._handle)
            workers = list(self._workers)
        try:
            for worker in workers:
                worker.join()
            with self._lock:
                self._api.library.mln_session_destroy(self._handle)
                self._handle = ctypes.c_void_p()
        finally:
            with self._lock:
                self._closing = False
                self._condition.notify_all()

    def _require_open(self) -> None:
        if not self._handle or self._closing:
            raise RuntimeError("session is closed")

    def __enter__(self) -> Session:
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def __del__(self) -> None:
        if hasattr(self, "_lock") and hasattr(self, "_handle"):
            self.close()
