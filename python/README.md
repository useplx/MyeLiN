# MyeLiN Runtime for Python

This package exposes the stable MyeLiN C ABI. Set `MLN_RUNTIME_LIBRARY` when
`libmln_runtime` is not installed in the platform library search path.

```python
from myelin import Metadata, Runtime

with Runtime("myelin.toml") as runtime:
    with runtime.session() as session:
        metadata = Metadata.read("application.mln")
        print(metadata.backends)
        print(metadata.requirements)
        session.load("application.mln", backend=metadata.backends[0])
        for chunk in session.stream(b"prompt"):
            print(chunk.decode(errors="replace"), end="", flush=True)
```

Package warnings are emitted through Python's `warnings` module by default.
`Session.set_diagnostic_handler()` accepts a callback returning `"continue"`,
`"ignore"`, or `"promote"` when an application needs explicit policy.

`Runtime.devices()` enumerates the drivers and physical devices reported by
the IREE HAL. After loading an artifact, `Session.devices()` reports which URI
was selected from each ordered alternative compiled into the artifact.
`Metadata.backends` is the source of truth for selectors: it contains the
normalized backends actually compiled into the `.mln`. Omitting `backend` from
`Session.load` leaves selection to the runtime scheduler.

```toml
[runtime]
memory = "8GiB"
scheduler = "adaptive"
variants = 8

[runtime.profile]
output = "captures/run-{capture}.ireeprof"

[backend.local]
device = "local"
ordinal = 0
threads = 8
```

When no backend table is present, the runtime discovers devices automatically
and follows the artifact's ordered alternatives. When one or more backend
tables are present, they are the enabled set; `device` and `ordinal` select the
physical IREE device for each compatible artifact target.

The runtime validates backend ownership, applies device selection, enables
IREE profiling only when requested, and constructs an isolated `local-task`
executor with the requested number of worker threads. Adaptive routing uses
per-entrypoint latency and in-flight work. `runtime.memory` is a hard
per-variant admission budget measured with IREE allocator statistics.

`Metadata` also exposes sections, public entrypoints, typed signatures (with
input dtypes and shapes), deployment, parameters, and ABI/platform
requirements. `Buffer` supports shaped multi-buffer invocation, state, and
assets through the same C ABI used by the C++ runtime.
