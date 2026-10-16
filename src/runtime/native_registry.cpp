#include "native_registry.hpp"
#include "mln/error.hpp"
#include "mln/runtime/errors.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace Mln {
    void Native::Registry::load(std::string_view path) {
        register_provider(Library::open(path), path);
    }

    void Native::Registry::load_memory(std::string_view name, std::span<const uint8_t> bytes) {
        register_provider(Library::open(name, bytes), std::format("package:{}", name));
    }

    void Native::Registry::register_provider(Library library, std::string_view origin) {

        using Function = const Mln::Sdk::Native::Module *(*)();
        void *raw = library.symbol("mln_native_module");
        if (!raw) {
            raise(Runtime::Errors::Native::Module::Symbol::Missing, origin, Library::error());
        }

        const Mln::Sdk::Native::Module *module = reinterpret_cast<Function>(raw)();
        if (!module) {
            raise(Runtime::Errors::Native::Module::Null, origin);
        }
        if (module->struct_size < sizeof(Mln::Sdk::Native::Module)) {
            raise(Runtime::Errors::Native::Module::Abi::Incompatible, origin, "<unknown>",
                  Mln::Sdk::Native::MIN_COMPATIBLE_ABI_VERSION,
                  Mln::Sdk::Native::ABI_VERSION, module->abi_version);
        }
        if (!module->name) {
            raise(Runtime::Errors::Native::Module::Name::Null, origin);
        }
        if (module->abi_version < Mln::Sdk::Native::MIN_COMPATIBLE_ABI_VERSION ||
            module->abi_version > Mln::Sdk::Native::ABI_VERSION) {
            std::string name = module->name;
            raise(Runtime::Errors::Native::Module::Abi::Incompatible, origin, name,
                  Mln::Sdk::Native::MIN_COMPATIBLE_ABI_VERSION, Mln::Sdk::Native::ABI_VERSION,
                  module->abi_version);
        }
        if (module->symbol_count > 0 && !module->symbols) {
            std::string name = module->name;
            raise(Runtime::Errors::Native::Symbol::Table::Null, origin, name);
        }

        std::vector<std::string> keys;
        keys.reserve(module->symbol_count);
        for (uint32_t i = 0; i < module->symbol_count; i++) {
            const Mln::Sdk::Native::Symbol &symbol = module->symbols[i];
            if (!symbol.name || !symbol.function) {
                std::string name = module->name;
                raise(Runtime::Errors::Native::Symbol::Invalid, origin, name, i);
            }
            if (symbol.signature &&
                ((symbol.signature->argument_count > 0 && !symbol.signature->arguments) ||
                 (symbol.signature->result_count > 0 && !symbol.signature->results))) {
                std::string name = module->name;
                raise(Runtime::Errors::Native::Symbol::Signature::Invalid, origin, name,
                      symbol.name);
            }
            std::string key(module->name);
            key += '.';
            key.append(symbol.name);
            if (symbols_.contains(key) || std::find(keys.begin(), keys.end(), key) != keys.end()) {
                raise(Runtime::Errors::Native::Symbol::Conflict, origin, key);
            }
            keys.push_back(std::move(key));
        }

        for (uint32_t i = 0; i < module->symbol_count; ++i)
            symbols_.emplace(keys[i], module->symbols[i]);

        libraries_.push_back(std::move(library));
    }

    const Mln::Sdk::Native::Symbol *Native::Registry::resolve(std::string_view module,
                                                              std::string_view symbol) const {
        std::string key(module);
        key += '.';
        key.append(symbol);
        const auto it = symbols_.find(key);
        return it != symbols_.end() ? &it->second : nullptr;
    }
} // namespace Mln
