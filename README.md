# MyeLiN

MyeLiN is the authoring SDK, runtime, and shared artifact contract used by PLeX.

It lets package authors combine tensor computation, portable orchestration,
native providers, state, and assets, then execute a self-contained
`.mln` artifact through one streaming interface.

## What MyeLiN provides

- a C++ SDK for tensor graphs, orchestration, and native contracts;
- contracts consumed by the SYNaPSe compiler for CPU, CUDA, and Vulkan targets;
- configurable model paths, entrypoints, and weight formats;
- a single-file `.mln` deployment artifact;
- runtime loading, state, assets, and streamed output;
- metadata and opt-in lifecycle events for external tooling;
- integration with SYNaPSe for package-based builds.

MyeLiN uses the official upstream IREE source pinned by this repository. No
patched IREE fork is required.

## Requirements

- Linux
- CMake 3.21 or newer
- a C++23 compiler
- Git
- Wasmtime 40 C/C++ development library (the CI reference is 40.0.0)
- sufficient disk space for IREE and LLVM
- optional CUDA or Vulkan development tools for those targets

## Build

Initialize dependencies:

```bash
git submodule update --init --recursive
```

Build the SDK and runtime:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --install build --prefix ~/.local
```

Applications using CMake can then consume the installed targets directly:

```cmake
find_package(MyeLiN 1.0 REQUIRED)

target_link_libraries(application PRIVATE MyeLiN::runtime)
```

Use `MyeLiN::sdk` when authoring model packages without embedding the Runtime,
or `MyeLiN::package` for tooling that only reads and writes `.mln` containers.

### Enable CUDA

```bash
cmake -S . -B build \
  -DMLN_ENABLE_CUDA=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Enable Vulkan

```bash
cmake -S . -B build \
  -DMLN_ENABLE_VULKAN=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Runtime-only deployment

```bash
cmake -S . -B build \
  -DMLN_BUILD_RUNTIME=ON \
  -DMLN_BUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Use SYNaPSe for application builds. The CMake build is for developing and
installing MyeLiN itself.

### Sanitizers and fuzzing

Untrusted `.mln` parsing has an opt-in Clang/libFuzzer target:

```bash
CC=clang CXX=clang++ cmake -S . -B build-fuzz \
  -DMLN_BUILD_RUNTIME=OFF \
  -DMLN_ENABLE_SANITIZERS=ON \
  -DMLN_BUILD_FUZZERS=ON
cmake --build build-fuzz -j8
ctest --test-dir build-fuzz
mkdir -p build-fuzz/corpus
cp build-fuzz/src/runtime/test-assets.mln build-fuzz/corpus/valid.mln
build-fuzz/src/runtime/mln_package_fuzzer -runs=10000 build-fuzz/corpus
```

The normal SDK and runtime builds do not include the fuzzer or sanitizer
runtime.

## Author a graph

A model is a tree of `Mln::Sdk::Module` objects. Submodule names become stable
paths that applications can use for policies and graph hooks.

```cpp
#include <mln/sdk/functional/multiply.hpp>
#include <mln/sdk/module.hpp>

class Square final : public Mln::Sdk::Module {
    public:
        Square() { name = "model"; }

        Mln::Sdk::Tensor forward(const Mln::Sdk::Tensor &input) {
            return operations.insert<Mln::Sdk::Functional::Multiply>(input, input);
        }
};
```

Use `modules.insert<T>()` and `modules.forward<T>()` to build reusable nested
models. Use `parameters` for checkpoint-backed weights.

See [SDK reference](https://useplx.github.io/api/mln-sdk/).

## Configure compilation

MyeLiN itself is a CMake project and does not contain or consume a
`synps.toml`. Applications built with SYNaPSe configure model behavior in their
own `synps.toml`:

```toml
[policies."model"]
dtype = "f16"

[policies."model".backend]
device = "local"
```

Policies inherit by path. Packages own their graph contracts and the root
`[entrypoint].package` selects the public `forward`. SYNaPSe validates the
composition and invokes its compiler, which consumes MyeLiN's shared SDK and
artifact contracts.

See [Compilation notes](https://useplx.github.io/notes/compilation/).

## Run an artifact

```cpp
#include <mln/runtime/runtime.hpp>

#include <cstdint>
#include <span>
#include <string_view>

Mln::Runtime runtime;
auto session = runtime.session();
session.load("build/app.mln");

constexpr std::string_view prompt = "hello";
Mln::Buffer input(prompt.size());
input.write(std::as_bytes(std::span(prompt)));

session.invoke(
    input,
    [](std::span<const uint8_t> output) {
        // Consume each streamed value here.
    });
```

Use the overload with an entrypoint name when the artifact exposes more than
one. The callback is synchronous and its byte span is valid only during the
call.

### C and Python

The versioned C ABI in `<mln/c/runtime.h>` is the language-neutral runtime
boundary. It uses opaque handles, returns status values instead of allowing C++
exceptions to cross the ABI, and preserves package diagnostics through
`mln_last_diagnostic()`. A session retains the runtime state it uses, so handle
destruction order cannot invalidate a live session. Calls on different sessions
inside the same runtime are serialized because they share runtime registries.
Independent runtime instances may execute concurrently.

The Python package is a thin wrapper over that ABI:

```bash
python -m pip install ./python
export MLN_RUNTIME_LIBRARY=/path/to/libmln_runtime.so
```

```python
from myelin import Runtime

with Runtime() as runtime, runtime.session("app.mln") as session:
    for chunk in session.stream("hello"):
        print(chunk.decode(errors="replace"), end="", flush=True)
```

`Session.stream()` forwards each runtime callback immediately; it does not wait
for the complete response. Closing a stream stops delivery of later chunks, but
the current ABI does not promise backend cancellation after dispatch.

See [Runtime reference](https://useplx.github.io/api/mln-runtime/).

## Package diagnostics

Framework and third-party packages use the same structured diagnostic channel.
Each package owns a domain and a local numeric catalog, so it can add conditions
without editing a central registry:

```cpp
inline constexpr Mln::Error Fallback{
    "acme.image", 12, "operation '{}' uses the portable implementation"};

Mln::warn_at(Fallback, "model.encoder", "resize");
```

Errors raised with `Mln::raise` or `Mln::raise_at` stop the active flow.
Warnings continue by default. `Mln::ScopedDiagnosticHandler` lets a compiler,
runtime, test, or application consume, ignore, or promote warnings to errors.
Diagnostics preserve severity, domain, code, message, and location across the
SYNaPSe compiler ABI.

## Choose weight formats

MyeLiN defines the codec interface; concrete formats are distributed by the
[Quantizations](https://github.com/useplx/Quantizations) repository. Applications depend
only on the formats used by their policies.

See [Quantization guide](https://useplx.github.io/guide/quantize-weights/).

## Current limitations

- One invocation at a time per session.
- Execution and output callbacks are synchronous.
- A loaded package remains available until its owning Session is destroyed.
- CPU is enabled by default; CUDA and Vulkan require explicit build options.
- Graph hooks are available. Unified Graph, Orchestration, and Native path
  hooking is still under development.
- Public C, C++, Python, artifact, and manifest contracts follow semantic
  versioning from v1.0 onward.

## Security

An `.mln` is an executable deployment artifact and may contain Native
providers or drivers. Structural validation is not authenticity verification;
production systems must authenticate artifacts before loading them. See
[`SECURITY.md`](SECURITY.md) for the trust model and reporting guidance.

## Documentation

Full documentation lives at **[https://useplx.github.io/tools/myelin/](https://useplx.github.io/tools/myelin/)**.

- [SDK reference](https://useplx.github.io/api/mln-sdk/) — define graphs, parameters, and orchestration
- [Compilation](https://useplx.github.io/notes/compilation/) — paths, devices, entrypoints, hooks
- [Runtime](https://useplx.github.io/api/mln-runtime/) — load a `.mln`, provide input, consume output
- [Packaging](https://useplx.github.io/guide/artifacts/) — assets, providers, artifact contents
- [Quantization](https://useplx.github.io/guide/quantize-weights/) — choose and configure weight formats
- [Runtime API](https://useplx.github.io/api/mln-runtime/) — sessions, loading, and execution
- [Artifact format](https://useplx.github.io/guide/artifacts/) — the `.mln` container, version 1


## License

MyeLiN is source-available under the
[PolyForm Noncommercial License 1.0.0](LICENSE). Noncommercial use is permitted
without charge. Commercial use requires a separate agreement with the copyright
holder.
