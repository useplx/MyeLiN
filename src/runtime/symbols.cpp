#include "symbols.hpp"
#include "mln/error.hpp"
#include "mln/runtime/errors.hpp"
#include "mln/runtime/runtime.hpp"

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Mln::Wasm {
    namespace {
        using wasmtime::Caller;
        using wasmtime::FuncType;
        using wasmtime::Result;
        using wasmtime::Span;
        using wasmtime::Trap;
        using wasmtime::Val;
        using wasmtime::ValKind;
        using wasmtime::ValType;

        FuncType i32_to_i32(int argc) {
            std::vector<ValType> params(argc, ValType(ValKind::I32));
            return FuncType::from_iters(params, std::vector<ValType>{ValType(ValKind::I32)});
        }

        FuncType i32_to_void(int argc) {
            std::vector<ValType> params(argc, ValType(ValKind::I32));
            return FuncType::from_iters(params, std::vector<ValType>{});
        }

        wasmtime::Span<uint8_t> memory_of(Caller &caller) {
            auto exp = caller.get_export("memory");
            if (!exp || !std::holds_alternative<wasmtime::Memory>(*exp))
                raise(Runtime::Errors::Wasm::Memory::Missing);
            return std::get<wasmtime::Memory>(*exp).data(caller.context());
        }

        std::string read_cstring(Caller &caller, uint32_t ptr) {
            auto mem = memory_of(caller);
            std::string s;
            while (ptr < mem.size() && mem[ptr] != 0) {
                s.push_back(static_cast<char>(mem[ptr]));
                ptr++;
            }
            return s;
        }
    } // namespace

    std::vector<Symbol> core_symbols(Runtime &runtime) {
        std::vector<Symbol> symbols;

        symbols.push_back({"myelin", "buffer_byte_at", i32_to_i32(2),
                           [&runtime](Caller, Span<const Val> args,
                                      Span<Val> results) -> Result<std::monostate, Trap> {
                               uint8_t value =
                                   runtime.byte_at(static_cast<uint32_t>(args[0].i32()),
                                                   static_cast<uint32_t>(args[1].i32()));
                               results[0] = Val(static_cast<int32_t>(value));
                               return std::monostate();
                           }});

        symbols.push_back({"myelin", "buffer_length", i32_to_i32(1),
                           [&runtime](Caller, Span<const Val> args,
                                      Span<Val> results) -> Result<std::monostate, Trap> {
                               uint32_t value =
                                   runtime.length(static_cast<uint32_t>(args[0].i32()));
                               results[0] = Val(static_cast<int32_t>(value));
                               return std::monostate();
                           }});

        symbols.push_back(
            {"myelin", "buffer_write_byte", i32_to_void(3),
             [&runtime](Caller, Span<const Val> args, Span<Val>) -> Result<std::monostate, Trap> {
                 runtime.write_byte(static_cast<uint32_t>(args[0].i32()),
                                    static_cast<uint32_t>(args[1].i32()),
                                    static_cast<uint8_t>(args[2].i32()));
                 return std::monostate();
             }});

        symbols.push_back({"myelin", "buffer_alloc", i32_to_i32(1),
                           [&runtime](Caller, Span<const Val> args,
                                      Span<Val> results) -> Result<std::monostate, Trap> {
                               uint32_t id = runtime.allocate(static_cast<uint32_t>(args[0].i32()));
                               results[0] = Val(static_cast<int32_t>(id));
                               return std::monostate();
                           }});

        symbols.push_back(
            {"myelin", "buffer_free", i32_to_void(1),
             [&runtime](Caller, Span<const Val> args, Span<Val>) -> Result<std::monostate, Trap> {
                 runtime.release(static_cast<uint32_t>(args[0].i32()));
                 return std::monostate();
             }});

        symbols.push_back({"myelin", "buffer_read", i32_to_void(4),
                           [&runtime](Caller caller, Span<const Val> args,
                                      Span<Val>) -> Result<std::monostate, Trap> {
                               auto mem = memory_of(caller);
                               uint32_t buffer = static_cast<uint32_t>(args[0].i32());
                               uint32_t offset = static_cast<uint32_t>(args[1].i32());
                               uint32_t dst = static_cast<uint32_t>(args[2].i32());
                               uint32_t len = static_cast<uint32_t>(args[3].i32());
                               runtime.read(buffer, offset, mem.data() + dst, len);
                               return std::monostate();
                           }});

        symbols.push_back({"myelin", "buffer_write", i32_to_void(4),
                           [&runtime](Caller caller, Span<const Val> args,
                                      Span<Val>) -> Result<std::monostate, Trap> {
                               auto mem = memory_of(caller);
                               uint32_t buffer = static_cast<uint32_t>(args[0].i32());
                               uint32_t offset = static_cast<uint32_t>(args[1].i32());
                               uint32_t src = static_cast<uint32_t>(args[2].i32());
                               uint32_t len = static_cast<uint32_t>(args[3].i32());
                               runtime.write(buffer, offset, mem.data() + src, len);
                               return std::monostate();
                           }});

        symbols.push_back({"myelin", "session_run", i32_to_i32(3),
                           [&runtime](Caller caller, Span<const Val> args,
                                      Span<Val> results) -> Result<std::monostate, Trap> {
                               uint32_t session = static_cast<uint32_t>(args[0].i32());
                               std::string path =
                                   read_cstring(caller, static_cast<uint32_t>(args[1].i32()));
                               uint32_t input = static_cast<uint32_t>(args[2].i32());
                               uint32_t output = runtime.execute(session, path, input);
                               results[0] = Val(static_cast<int32_t>(output));
                               return std::monostate();
                           }});

        symbols.push_back(
            {"myelin", "session_run_many", i32_to_i32(6),
             [&runtime](Caller caller, Span<const Val> args,
                        Span<Val> results) -> Result<std::monostate, Trap> {
                 const uint32_t session = static_cast<uint32_t>(args[0].i32());
                 const std::string path =
                     read_cstring(caller, static_cast<uint32_t>(args[1].i32()));
                 const uint32_t inputs_ptr = static_cast<uint32_t>(args[2].i32());
                 const uint32_t input_count = static_cast<uint32_t>(args[3].i32());
                 const uint32_t outputs_ptr = static_cast<uint32_t>(args[4].i32());
                 const uint32_t output_capacity = static_cast<uint32_t>(args[5].i32());
                 auto memory = memory_of(caller);
                 const uint64_t inputs_end = static_cast<uint64_t>(inputs_ptr) +
                                             static_cast<uint64_t>(input_count) * sizeof(uint32_t);
                 const uint64_t outputs_end =
                     static_cast<uint64_t>(outputs_ptr) +
                     static_cast<uint64_t>(output_capacity) * sizeof(uint32_t);
                 if (inputs_end > memory.size() || outputs_end > memory.size())
                     raise(Runtime::Errors::Abi::Memory::Array::Out::Of::Bounds);

                 std::vector<uint32_t> input_ids(input_count);
                 if (input_count != 0)
                     std::memcpy(input_ids.data(), memory.data() + inputs_ptr,
                                 input_ids.size() * sizeof(uint32_t));
                 const std::vector<uint32_t> output_ids = runtime.execute(session, path, input_ids);
                 if (output_ids.size() > output_capacity) {
                     for (uint32_t output : output_ids)
                         runtime.release(output);
                     raise(Runtime::Errors::Abi::Output::Capacity::Insufficient, output_capacity,
                           output_ids.size());
                 }
                 if (!output_ids.empty())
                     std::memcpy(memory.data() + outputs_ptr, output_ids.data(),
                                 output_ids.size() * sizeof(uint32_t));
                 results[0] = Val(static_cast<int32_t>(output_ids.size()));
                 return std::monostate();
             }});

        symbols.push_back({"myelin", "session_state", i32_to_i32(3),
                           [&runtime](Caller caller, Span<const Val> args,
                                      Span<Val> results) -> Result<std::monostate, Trap> {
                               const uint32_t session = static_cast<uint32_t>(args[0].i32());
                               const std::string name =
                                   read_cstring(caller, static_cast<uint32_t>(args[1].i32()));
                               const uint32_t bytes = static_cast<uint32_t>(args[2].i32());
                               const uint32_t state = runtime.state(session, name, bytes);
                               results[0] = Val(static_cast<int32_t>(state));
                               return std::monostate();
                           }});

        symbols.push_back({"myelin", "session_set_state", i32_to_i32(3),
                           [&runtime](Caller caller, Span<const Val> args,
                                      Span<Val> results) -> Result<std::monostate, Trap> {
                               const uint32_t session = static_cast<uint32_t>(args[0].i32());
                               const std::string name =
                                   read_cstring(caller, static_cast<uint32_t>(args[1].i32()));
                               const uint32_t buffer = static_cast<uint32_t>(args[2].i32());
                               const uint32_t state = runtime.set_state(session, name, buffer);
                               results[0] = Val(static_cast<int32_t>(state));
                               return std::monostate();
                           }});

        symbols.push_back({"myelin", "session_asset", i32_to_i32(2),
                           [&runtime](Caller caller, Span<const Val> args,
                                      Span<Val> results) -> Result<std::monostate, Trap> {
                               const uint32_t session = static_cast<uint32_t>(args[0].i32());
                               const std::string name =
                                   read_cstring(caller, static_cast<uint32_t>(args[1].i32()));
                               const uint32_t asset = runtime.asset(session, name);
                               results[0] = Val(static_cast<int32_t>(asset));
                               return std::monostate();
                           }});

        symbols.push_back(
            {"myelin", "session_emit", i32_to_void(3),
             [&runtime](Caller, Span<const Val> args, Span<Val>) -> Result<std::monostate, Trap> {
                 runtime.emit(static_cast<uint32_t>(args[0].i32()),
                              static_cast<uint32_t>(args[1].i32()),
                              static_cast<uint32_t>(args[2].i32()));
                 return std::monostate();
             }});

        return symbols;
    }
} // namespace Mln::Wasm
