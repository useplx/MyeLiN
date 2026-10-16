#pragma once

#include "mln/runtime/errors.hpp"

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <wasmtime.hh>

namespace Mln::Wasm {
    struct Symbol {
            std::string module;
            std::string symbol;
            wasmtime::FuncType type;
            std::function<wasmtime::Result<std::monostate, wasmtime::Trap>(
                wasmtime::Caller, wasmtime::Span<const wasmtime::Val>,
                wasmtime::Span<wasmtime::Val>)>
                func;
    };

    class Vm {
        private:
            struct Module {
                    wasmtime::Instance instance;
                    std::unordered_map<std::string, wasmtime::Func> funcs;
            };

            std::unordered_map<std::string, Module> modules;

        public:
            struct Config {
                    wasmtime::Config engine;
                    wasmtime::WasiConfig wasi;
                    std::vector<Wasm::Symbol> symbols;
            };

            wasmtime::Engine engine;
            wasmtime::Store store;
            wasmtime::Linker linker;

            explicit Vm(Config config)
                : engine(std::move(config.engine)), store(engine), linker(engine) {
                if (auto ok = linker.define_wasi(); !ok)
                    raise(Runtime::Errors::Wasm::Vm::Wasi::Definition::Failed,
                          std::string(ok.err().message()));

                if (auto ok = store.context().set_wasi(std::move(config.wasi)); !ok)
                    raise(Runtime::Errors::Wasm::Vm::Wasi::Configuration::Failed,
                          std::string(ok.err().message()));

                for (auto &n : config.symbols)
                    if (auto ok = linker.func_new(n.module, n.symbol, n.type, std::move(n.func));
                        !ok)
                        raise(Runtime::Errors::Wasm::Vm::Host::Symbol::Definition::Failed,
                              std::string(ok.err().message()));
            }

            wasmtime::Module compile(std::vector<uint8_t> buffer) {
                auto module = wasmtime::Module::compile(engine, buffer);
                if (!module)
                    raise(Runtime::Errors::Wasm::Vm::Compilation::Failed,
                          std::string(module.err().message()));
                return module.ok();
            }

            void instantiate(const std::string &id, const wasmtime::Module &module) {
                if (modules.contains(id))
                    raise(Runtime::Errors::Wasm::Vm::Module::Already::Loaded, id);

                auto instance = linker.instantiate(store, module);
                if (!instance)
                    raise(Runtime::Errors::Wasm::Vm::Instantiation::Failed,
                          instance.err().message());

                wasmtime::Instance loaded = instance.ok();

                if (auto exported = loaded.get(store, "_initialize")) {
                    if (!std::holds_alternative<wasmtime::Func>(*exported))
                        raise(Runtime::Errors::Wasm::Vm::Initialize::Not::Function, id);
                    auto initialized = std::get<wasmtime::Func>(*exported).call(
                        store, std::vector<wasmtime::Val>{});
                    if (!initialized)
                        raise(Runtime::Errors::Wasm::Vm::Initialization::Failed,
                              initialized.err().message());
                }

                modules.emplace(id, Module{std::move(loaded), {}});
            }

            void load(const std::string &id, std::vector<uint8_t> buffer) {
                instantiate(id, compile(buffer));
            }

            std::vector<wasmtime::Val> call(const std::string &id, const std::string &func,
                                            const std::vector<wasmtime::Val> &args = {}) {
                auto it = modules.find(id);
                if (it == modules.end())
                    raise(Runtime::Errors::Wasm::Vm::Module::Not::Loaded, id);

                auto &[instance, funcs] = it->second;

                auto fit = funcs.find(func);
                if (fit == funcs.end()) {
                    auto exp = instance.get(store, func);
                    if (!exp || !std::holds_alternative<wasmtime::Func>(*exp))
                        raise(Runtime::Errors::Wasm::Vm::Function::Not::Exported, func, id);
                    fit = funcs.emplace(func, std::get<wasmtime::Func>(*exp)).first;
                }

                auto results = fit->second.call(store, args);
                if (!results)
                    raise(Runtime::Errors::Wasm::Vm::Invocation::Failed, results.err().message());

                return results.ok();
            }
    };
} // namespace Mln::Wasm
