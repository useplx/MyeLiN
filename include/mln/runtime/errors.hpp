#pragma once

#include "mln/error.hpp"
#include "mln/runtime/runtime.hpp"

namespace Mln {
    struct Runtime::Errors {
            struct Iree {
                    struct File {
                            struct Wrap {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 0,
                                        "IREE failed to wrap an in-memory file: {}"};
                            };
                    };
                    struct Parameter {
                            struct Index {
                                    struct Create {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 1,
                                                "IREE failed to create a parameter index: {}"};
                                    };
                                    struct Parse {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 2,
                                                "IREE failed to parse a parameter index: {}"};
                                    };
                                    struct Entry {
                                            struct Read {
                                                    static inline constexpr Mln::Error Failed{
                                                        Mln::Range::Runtime + 3,
                                                        "IREE failed to read a parameter entry: "
                                                        "{}"};
                                            };
                                    };
                            };
                            struct Provider {
                                    struct Create {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 7,
                                                "IREE failed to create a parameter provider: {}"};
                                    };
                            };
                            struct Module {
                                    struct Create {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 8,
                                                "IREE failed to create a parameter module: {}"};
                                    };
                            };
                            struct File {
                                    struct Open {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 36,
                                                "IREE failed to open the parameter file: {}"};
                                    };
                            };
                    };
                    struct Proactor {
                            struct Pool {
                                    struct Create {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 4,
                                                "IREE failed to create a proactor pool: {}"};
                                    };
                            };
                    };
                    struct Device {
                            struct Create {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 5,
                                        "IREE failed to create the requested device: {}"};
                            };
                            struct Trim {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 45,
                                        "IREE failed to release idle device resources: {}"};
                            };
                    };
                    struct Session {
                            struct Create {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 6,
                                        "IREE failed to create a runtime session: {}"};
                            };
                            struct Module {
                                    struct Append {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 9,
                                                "IREE failed to append a session module: {}"};
                                    };
                            };
                    };
                    struct Bytecode {
                            struct Memory {
                                    struct Append {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 34,
                                                "IREE failed to append in-memory VM bytecode: {}"};
                                    };
                            };
                            struct File {
                                    struct Append {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 37,
                                                "IREE failed to append VM bytecode file: {}"};
                                    };
                            };
                    };
                    struct Runtime {
                            struct Instance {
                                    struct Create {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 35,
                                                "IREE failed to create the runtime instance: {}"};
                                    };
                            };
                    };
                    struct Transfer {
                            struct Device {
                                    struct To {
                                            struct Host {
                                                    static inline constexpr Mln::Error Failed{
                                                        Mln::Range::Runtime + 38,
                                                        "IREE device-to-host transfer failed: {}"};
                                            };
                                    };
                            };
                            struct Host {
                                    struct To {
                                            struct Device {
                                                    static inline constexpr Mln::Error Failed{
                                                        Mln::Range::Runtime + 39,
                                                        "IREE host-to-device transfer failed: {}"};
                                            };
                                    };
                            };
                    };
                    struct Call {
                            struct Initialize {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 40,
                                        "IREE failed to initialize the graph call: {}"};
                            };
                            struct Input {
                                    struct Allocate {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 41,
                                                "IREE failed to allocate an input view: {}"};
                                    };
                                    struct Push {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 42,
                                                "IREE failed to attach an input view: {}"};
                                    };
                            };
                            struct Invoke {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 43,
                                        "IREE graph invocation failed: {}"};
                            };
                            struct Output {
                                    struct Pop {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 44,
                                                "IREE failed to read a graph output: {}"};
                                    };
                            };
                    };
            };
            struct Wasm {
                    struct Vm {
                            struct Wasi {
                                    struct Definition {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 19,
                                                "Wasm: failed to define WASI: {}"};
                                    };
                                    struct Configuration {
                                            static inline constexpr Mln::Error Failed{
                                                Mln::Range::Runtime + 20,
                                                "Wasm: failed to configure WASI: {}"};
                                    };
                            };
                            struct Host {
                                    struct Symbol {
                                            struct Definition {
                                                    static inline constexpr Mln::Error Failed{
                                                        Mln::Range::Runtime + 21,
                                                        "Wasm: failed to define host symbol: {}"};
                                            };
                                    };
                            };
                            struct Compilation {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 22, "Wasm: compilation failed: {}"};
                            };
                            struct Module {
                                    struct Already {
                                            static inline constexpr Mln::Error Loaded{
                                                Mln::Range::Runtime + 23,
                                                "Wasm: module is already loaded: '{}'"};
                                    };
                                    struct Not {
                                            static inline constexpr Mln::Error Loaded{
                                                Mln::Range::Runtime + 27,
                                                "Wasm: module is not loaded: '{}'"};
                                    };
                            };
                            struct Instantiation {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 24, "Wasm: instantiation failed: {}"};
                            };
                            struct Initialize {
                                    struct Not {
                                            static inline constexpr Mln::Error Function{
                                                Mln::Range::Runtime + 25,
                                                "Wasm: _initialize export is not a function in "
                                                "module '{}'"};
                                    };
                            };
                            struct Initialization {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 26, "Wasm: initialization failed: {}"};
                            };
                            struct Function {
                                    struct Not {
                                            static inline constexpr Mln::Error Exported{
                                                Mln::Range::Runtime + 28,
                                                "Wasm: function '{}' is not exported by module '{}'"};
                                    };
                            };
                            struct Invocation {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 29, "Wasm: invocation failed: {}"};
                            };
                    };
                    struct Value {
                            struct Argument {
                                    struct Type {
                                            static inline constexpr Mln::Error Unsupported{
                                                Mln::Range::Runtime + 10,
                                                "native symbol: unsupported Wasm argument type"};
                                    };
                            };
                            struct Result {
                                    struct Type {
                                            static inline constexpr Mln::Error Unsupported{
                                                Mln::Range::Runtime + 11,
                                                "native symbol: unsupported Wasm result type"};
                                    };
                            };
                            struct Signature {
                                    struct Type {
                                            static inline constexpr Mln::Error Unsupported{
                                                Mln::Range::Runtime + 12,
                                                "native symbol: unsupported Wasm type in signature"};
                                    };
                            };
                    };
                    struct Memory {
                            static inline constexpr Mln::Error Missing{
                                Mln::Range::Runtime + 13, "Wasm module does not export linear memory"};
                    };
                    struct Module {
                            static inline constexpr Mln::Error Duplicated{
                                Mln::Range::Runtime + 14, "Package: duplicate Wasm module '{}'"};
                            static inline constexpr Mln::Error Unknown{
                                Mln::Range::Runtime + 15, "Runtime: unknown Wasm module '{}'"};
                            static inline constexpr Mln::Error Missing{
                                Mln::Range::Runtime + 16, "Package: no executable Wasm module"};
                    };
                    struct Import {
                            struct Not {
                                    static inline constexpr Mln::Error Function{
                                        Mln::Range::Runtime + 17,
                                        "import '{}.{}' is not a function"};
                            };
                    };
                    struct Link {
                            static inline constexpr Mln::Error Failed{
                                Mln::Range::Runtime + 18, "Wasm: failed to link import: {}"};
                    };
            };
            struct Native {
                    struct Signature {
                            struct Arity {
                                    static inline constexpr Mln::Error Mismatch{
                                        Mln::Range::Runtime + 30,
                                        "native symbol '{}': incompatible signature; Wasm "
                                        "declared {} argument(s) and {} result(s), provider "
                                        "exported {} argument(s) and {} result(s)"};
                            };
                            struct Argument {
                                    static inline constexpr Mln::Error Mismatch{
                                        Mln::Range::Runtime + 31,
                                        "native symbol '{}': incompatible argument {}; Wasm "
                                        "expects {}, provider exports {}"};
                            };
                            struct Result {
                                    static inline constexpr Mln::Error Mismatch{
                                        Mln::Range::Runtime + 32,
                                        "native symbol '{}': incompatible result {}; Wasm "
                                        "expects {}, provider exports {}"};
                            };
                            struct Symbol {
                                    static inline constexpr Mln::Error Missing{
                                        Mln::Range::Runtime + 33,
                                        "native symbol not found: {}.{}"};
                            };
                    };
                    struct Library {
                            struct Load {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Native + 0,
                                        "failed to load native provider '{}': {}"};
                            };
                            struct Memfd {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Native + 1,
                                        "memfd_create failed for '{}': {}"};
                                    static inline constexpr Mln::Error Unsupported{
                                        Mln::Range::Native + 4,
                                        "embedded native providers require memfd on Linux"};
                            };
                            struct Materialize {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Native + 2,
                                        "failed to materialize provider '{}': {}"};
                            };
                            struct Write {
                                    static inline constexpr Mln::Error Incomplete{
                                        Mln::Range::Native + 3,
                                        "incomplete write for provider '{}'"};
                            };
                    };
                    struct Module {
                            static inline constexpr Mln::Error Null{
                                Mln::Range::Native + 11,
                                "'{}': mln_native_module() returned nullptr"};
                            struct Symbol {
                                    static inline constexpr Mln::Error Missing{
                                        Mln::Range::Native + 10,
                                        "'{}' does not export mln_native_module: {}"};
                            };
                            struct Abi {
                                    static inline constexpr Mln::Error Incompatible{
                                        Mln::Range::Native + 13,
                                        "'{}' (module '{}'): incompatible abi_version "
                                        "(supported {}..{}, received {})"};
                            };
                            struct Name {
                                    static inline constexpr Mln::Error Null{
                                        Mln::Range::Native + 12,
                                        "'{}': Mln::Sdk::Native::Module::name is null"};
                            };
                    };
                    struct Symbol {
                            static inline constexpr Mln::Error Invalid{
                                Mln::Range::Native + 21,
                                "'{}' (module '{}'): invalid Mln::Sdk::Native::Symbol "
                                "(null name or function) at index {}"};
                            static inline constexpr Mln::Error Conflict{
                                Mln::Range::Native + 23, "'{}': conflicting native symbol '{}'"};
                            struct Table {
                                    static inline constexpr Mln::Error Null{
                                        Mln::Range::Native + 20,
                                        "'{}' (module '{}'): symbols is null with symbol_count > 0"};
                            };
                            struct Signature {
                                    static inline constexpr Mln::Error Invalid{
                                        Mln::Range::Native + 22,
                                        "'{}' (module '{}'): invalid Native::Signature on symbol "
                                        "'{}'"};
                            };
                    };
            };
            struct Session {
                    static inline constexpr Mln::Error Unknown{Mln::Range::Runtime + 53,
                                                               "Runtime: unknown Session"};
                    struct Device {
                            static inline constexpr Mln::Error Missing{
                                Mln::Range::Runtime + 50,
                                "Runtime: a Session requires at least one device"};
                            struct Count {
                                    static inline constexpr Mln::Error Unsupported{
                                        Mln::Range::Runtime + 51,
                                        "Runtime: the official IREE API supports exactly one "
                                        "device per Session"};
                            };
                            struct Deployment {
                                    static inline constexpr Mln::Error Unknown{
                                        Mln::Range::Runtime + 52,
                                        "Runtime: deployment references an unknown device"};
                            };
                            struct Backend {
                                    static inline constexpr Mln::Error Unavailable{
                                        Mln::Range::Runtime + 56,
                                        "Runtime: backend '{}' was not compiled into this artifact"};
                            };
                    };
                    struct Asset {
                            static inline constexpr Mln::Error Unavailable{
                                Mln::Range::Runtime + 54,
                                "Runtime: Session does not belong to a Package with assets"};
                    };
                    struct Already {
                            static inline constexpr Mln::Error Invoking{
                                Mln::Range::Runtime + 55,
                                "Runtime: this Session already has an active invocation"};
                    };
            };
            struct Buffer {
                    static inline constexpr Mln::Error Null{Mln::Range::Runtime + 70,
                                                            "Runtime: null Buffer"};
                    static inline constexpr Mln::Error Unknown{Mln::Range::Runtime + 71,
                                                               "Runtime: unknown Buffer"};
                    struct Abi {
                            static inline constexpr Mln::Error Overflow{
                                Mln::Range::Runtime + 72, "Runtime: Buffer exceeds the 32-bit ABI"};
                    };
                    struct Device {
                            struct Without {
                                    static inline constexpr Mln::Error Placement{
                                        Mln::Range::Runtime + 73,
                                        "Runtime: device Buffer has no placement"};
                            };
                    };
                    struct Read {
                            static inline constexpr Mln::Error Only{
                                Mln::Range::Runtime + 76, "Runtime: mapped asset is read-only"};
                            struct Out {
                                    struct Of {
                                            static inline constexpr Mln::Error Bounds{
                                                Mln::Range::Runtime + 74,
                                                "Runtime: read is outside the Buffer"};
                                    };
                            };
                    };
                    struct Write {
                            struct Out {
                                    struct Of {
                                            static inline constexpr Mln::Error Bounds{
                                                Mln::Range::Runtime + 75,
                                                "Runtime: write is outside the Buffer"};
                                    };
                            };
                    };
            };
            struct State {
                    struct Owned {
                            struct By {
                                    static inline constexpr Mln::Error Session{
                                        Mln::Range::Runtime + 90,
                                        "Runtime: State belongs to a Session and cannot be released "
                                        "as a Buffer"};
                            };
                    };
                    struct Name {
                            static inline constexpr Mln::Error Empty{
                                Mln::Range::Runtime + 91, "Runtime: State name is empty"};
                    };
                    struct Size {
                            static inline constexpr Mln::Error Mismatch{
                                Mln::Range::Runtime + 92,
                                "Runtime: State '{}' already exists with a different size"};
                    };
                    struct Buffer {
                            struct Already {
                                    static inline constexpr Mln::Error Owned{
                                        Mln::Range::Runtime + 93,
                                        "Runtime: Buffer already belongs to another State"};
                            };
                    };
            };
            struct Graph {
                    struct Dtype {
                            static inline constexpr Mln::Error Unknown{
                                Mln::Range::Runtime + 110, "Package: unknown dtype in '{}'"};
                    };
                    struct Input {
                            struct Count {
                                    static inline constexpr Mln::Error Mismatch{
                                        Mln::Range::Runtime + 111,
                                        "Runtime: input count is incompatible with '{}'"};
                            };
                    };
                    struct Byte {
                            struct Count {
                                    struct Not {
                                            static inline constexpr Mln::Error Divisible{
                                                Mln::Range::Runtime + 112,
                                                "Runtime: byte count is not divisible by dtype in '{}'"};
                                    };
                            };
                    };
                    struct Multiple {
                            struct Dynamic {
                                    static inline constexpr Mln::Error Dimensions{
                                        Mln::Range::Runtime + 113,
                                        "Runtime: more than one dynamic dimension in '{}'"};
                            };
                    };
                    struct Dynamic {
                            struct Shape {
                                    static inline constexpr Mln::Error Mismatch{
                                        Mln::Range::Runtime + 114,
                                        "Runtime: size does not match the shape of '{}'"};
                            };
                    };
                    struct Static {
                            struct Shape {
                                    static inline constexpr Mln::Error Mismatch{
                                        Mln::Range::Runtime + 115,
                                        "Runtime: size does not match the static shape of "
                                        "'{}'"};
                            };
                    };
                    struct Invocation {
                            static inline constexpr Mln::Error Failed{
                                Mln::Range::Runtime + 116, "Runtime: entrypoint '{}' failed: {}"};
                    };
                    struct Single {
                            struct Output {
                                    static inline constexpr Mln::Error Expected{
                                        Mln::Range::Runtime + 117,
                                        "Runtime: invocation expected exactly one result"};
                            };
                    };
                    struct Specialization {
                            static inline constexpr Mln::Error Missing{
                                Mln::Range::Runtime + 118,
                                "Runtime: no specialization accepts the inputs of '{}'"};
                    };
            };
            struct Stream {
                    struct Callback {
                            static inline constexpr Mln::Error Empty{
                                Mln::Range::Runtime + 130, "Runtime: output callback is empty"};
                    };
                    struct Offset {
                            struct Out {
                                    struct Of {
                                            static inline constexpr Mln::Error Bounds{
                                                Mln::Range::Runtime + 131,
                                                "Runtime: emission offset is outside the Buffer"};
                                    };
                            };
                    };
            };
            struct Entrypoint {
                    static inline constexpr Mln::Error Unknown{
                        Mln::Range::Runtime + 141,
                        "Runtime: unknown executable entrypoint '{}'"};
                    struct Name {
                            static inline constexpr Mln::Error Required{
                                Mln::Range::Runtime + 140,
                                "Runtime: Package has multiple entrypoints; provide a name"};
                    };
                    struct Section {
                            static inline constexpr Mln::Error Missing{
                                Mln::Range::Runtime + 142,
                                "Package: an executable-entrypoint section is required"};
                    };
                    struct Module {
                            static inline constexpr Mln::Error Missing{
                                Mln::Range::Runtime + 143,
                                "Package: entrypoint '{}' references missing Wasm module '{}'"};
                    };
            };
            struct Hook {
                    struct Module {
                            static inline constexpr Mln::Error Missing{
                                Mln::Range::Runtime + 144,
                                "Package: hook at '{}' references missing Wasm module '{}'"};
                    };
            };
            struct Package {
                    static inline constexpr Mln::Error Unknown{Mln::Range::Runtime + 167,
                                                               "Runtime: unknown Package"};
                    struct Path {
                            struct Inspection {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 160,
                                        "Package: could not inspect '{}': {}"};
                            };
                    };
                    struct Not {
                            static inline constexpr Mln::Error File{
                                Mln::Range::Runtime + 161, "Package: '{}' is not a .mln file"};
                    };
                    struct Parameters {
                            static inline constexpr Mln::Error Missing{
                                Mln::Range::Runtime + 162, "Package: no parameter archive"};
                    };
                    struct Weight {
                            struct Plan {
                                    static inline constexpr Mln::Error Missing{
                                        Mln::Range::Runtime + 163,
                                        "Package: typed weight plan is missing"};
                            };
                    };
                    struct Parameter {
                            static inline constexpr Mln::Error Duplicated{
                                Mln::Range::Runtime + 164,
                                "Package: duplicate parameter across archives: '{}'"};
                    };
                    struct Deployment {
                            static inline constexpr Mln::Error Duplicated{
                                Mln::Range::Runtime + 165,
                                "Package: multiple deployment sections"};
                    };
                    struct Signature {
                            static inline constexpr Mln::Error Duplicated{
                                Mln::Range::Runtime + 166,
                                "Package: multiple signature sections"};
                    };
            };
            struct Asset {
                    struct Abi {
                            static inline constexpr Mln::Error Overflow{
                                Mln::Range::Runtime + 180, "Runtime: asset exceeds the Buffer ABI"};
                    };
            };
            struct Irpa {
                    struct Metadata {
                            static inline constexpr Mln::Error Missing{
                                Mln::Range::Runtime + 190,
                                "IRPA: quantization metadata is missing for '{}'"};
                            struct Name {
                                    static inline constexpr Mln::Error Mismatch{
                                        Mln::Range::Runtime + 191,
                                        "IRPA: metadata belongs to '{}', not '{}'"};
                            };
                            struct Size {
                                    static inline constexpr Mln::Error Mismatch{
                                        Mln::Range::Runtime + 192,
                                        "IRPA: size does not match metadata for '{}'"};
                            };
                    };
            };
            struct Abi {
                    struct Output {
                            struct Capacity {
                                    static inline constexpr Mln::Error Insufficient{
                                        Mln::Range::Runtime + 200,
                                        "session_run: capacity {} is insufficient for {} results"};
                            };
                    };
                    struct Memory {
                            struct Array {
                                    struct Out {
                                            struct Of {
                                                    static inline constexpr Mln::Error Bounds{
                                                        Mln::Range::Runtime + 201,
                                                        "session_run: array is outside Wasm memory"};
                                            };
                                    };
                            };
                    };
            };
            struct Metadata {
                    struct Devices {
                            static inline constexpr Mln::Error Empty{
                                Mln::Range::Package + 40, "package devices section is empty"};
                    };
                    struct Section {
                            static inline constexpr Mln::Error Duplicated{
                                Mln::Range::Package + 41,
                                "Package: multiple sections of the same metadata type"};
                    };
                    struct Entrypoint {
                            static inline constexpr Mln::Error Invalid{
                                Mln::Range::Package + 50,
                                "Package: invalid executable entrypoint"};
                            static inline constexpr Mln::Error Duplicated{
                                Mln::Range::Package + 51,
                                "Package: duplicate executable entrypoint: {}"};
                            static inline constexpr Mln::Error None{
                                Mln::Range::Package + 52,
                                "Package: no executable entrypoint declared"};
                            struct Signature {
                                    static inline constexpr Mln::Error Invalid{
                                        Mln::Range::Package + 53,
                                        "Package: invalid entrypoint signature"};
                            };
                    };
                    struct Hook {
                            static inline constexpr Mln::Error Invalid{
                                Mln::Range::Package + 54, "Package: invalid hook route"};
                    };
                    struct Specialization {
                            static inline constexpr Mln::Error Duplicated{
                                Mln::Range::Package + 64, "Package: duplicate specialization: {}"};
                            struct Symbol {
                                    static inline constexpr Mln::Error Missing{
                                        Mln::Range::Package + 60,
                                        "Package: specialization symbol is missing"};
                                    static inline constexpr Mln::Error Invalid{
                                        Mln::Range::Package + 61,
                                        "Package: invalid specialization symbol"};
                            };
                            struct Argument {
                                    static inline constexpr Mln::Error Invalid{
                                        Mln::Range::Package + 62,
                                        "Package: invalid typed argument at {}"};
                            };
                            struct Shape {
                                    static inline constexpr Mln::Error Invalid{
                                        Mln::Range::Package + 63, "Package: invalid shape at {}"};
                            };
                    };
                    struct Deployment {
                            static inline constexpr Mln::Error Invalid{
                                Mln::Range::Package + 71, "Package: invalid deployment"};
                            struct Line {
                                    static inline constexpr Mln::Error Empty{
                                        Mln::Range::Package + 70,
                                        "Package: deployment contains an empty line"};
                            };
                            struct Device {
                                    static inline constexpr Mln::Error Invalid{
                                        Mln::Range::Package + 72,
                                        "Package: invalid device in deployment"};
                            };
                            struct Entrypoint {
                                    static inline constexpr Mln::Error Duplicated{
                                        Mln::Range::Package + 73,
                                        "Package: duplicate entrypoint in deployment: {}"};
                            };
                    };
            };
            struct Return {
                    struct Bytes {
                            static inline constexpr Mln::Error Trailing{
                                Mln::Range::Runtime + 210,
                                "Runtime return contains {} trailing bytes"};
                    };
            };
            struct Driver {
                    struct Module {
                            struct Symbol {
                                    static inline constexpr Mln::Error Missing{
                                        Mln::Range::Runtime + 220,
                                        "Runtime driver '{}' does not export mln_runtime_driver: {}"};
                            };
                            static inline constexpr Mln::Error Null{
                                Mln::Range::Runtime + 221,
                                "Runtime driver '{}' returned a null module"};
                            struct Name {
                                    static inline constexpr Mln::Error Missing{
                                        Mln::Range::Runtime + 222,
                                        "Runtime driver '{}' has no module name"};
                            };
                            struct Abi {
                                    static inline constexpr Mln::Error Incompatible{
                                        Mln::Range::Runtime + 223,
                                        "Runtime driver '{}' uses ABI {}, expected {}"};
                            };
                            struct Register {
                                    static inline constexpr Mln::Error Failed{
                                        Mln::Range::Runtime + 224,
                                        "Runtime driver '{}' failed to register: {}"};
                            };
                            struct Definition {
                                    static inline constexpr Mln::Error Conflicting{
                                        Mln::Range::Runtime + 225,
                                        "Runtime driver '{}' was loaded with conflicting bytes"};
                            };
                    };
            };
            struct Configuration {
                    struct Parse {
                            static inline constexpr Mln::Error Failed{
                                Mln::Range::Runtime + 230,
                                "failed to parse runtime configuration '{}': {}"};
                    };
                    struct Key {
                            static inline constexpr Mln::Error Unknown{
                                Mln::Range::Runtime + 231,
                                "unknown runtime configuration key '{}' at '{}'"};
                    };
                    struct Type {
                            static inline constexpr Mln::Error Invalid{
                                Mln::Range::Runtime + 238,
                                "invalid runtime configuration type at '{}'"};
                    };
                    struct Memory {
                            static inline constexpr Mln::Error Invalid{
                                Mln::Range::Runtime + 232,
                                "invalid runtime memory limit '{}'"};
                    };
                    struct Scheduler {
                            static inline constexpr Mln::Error Invalid{
                                Mln::Range::Runtime + 233,
                                "invalid runtime scheduler '{}'"};
                    };
                    struct Backend {
                            static inline constexpr Mln::Error Unknown{
                                Mln::Range::Runtime + 234,
                                "runtime backend '{}' is not registered"};
                            static inline constexpr Mln::Error Invalid{
                                Mln::Range::Runtime + 235,
                                "runtime backend '{}' must be a table"};
                            struct Ordinal {
                                    static inline constexpr Mln::Error Invalid{
                                        Mln::Range::Runtime + 236,
                                        "runtime backend '{}' has an invalid ordinal"};
                            };
                            struct Threads {
                                    static inline constexpr Mln::Error Invalid{
                                        Mln::Range::Runtime + 237,
                                        "runtime backend '{}' has invalid threads"};
                            };
                    };
            };
    };
} // namespace Mln
