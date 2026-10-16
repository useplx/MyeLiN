#include "driver_registry.hpp"

#include "mln/error.hpp"
#include "mln/runtime/errors.hpp"
#include "mln/sdk/driver/module.hpp"

#include <array>
#include <format>
#include <set>
#include <string>
#include <utility>

namespace Mln::Driver {
    namespace {
        std::size_t fingerprint(std::span<const std::uint8_t> bytes) noexcept {
            std::size_t value = sizeof(std::size_t) == 8 ? 1469598103934665603ULL : 2166136261U;
            const std::size_t prime = sizeof(std::size_t) == 8 ? 1099511628211ULL : 16777619U;
            for (const std::uint8_t byte : bytes) {
                value ^= byte;
                value *= prime;
            }
            return value;
        }
    } // namespace

    Registry::Registry(iree_hal_driver_registry_t &registry) noexcept : registry_(&registry) {}

    void Registry::load(std::string_view name, std::span<const std::uint8_t> bytes,
                        const Configuration::Runtime &configuration) {
        const std::size_t definition = fingerprint(bytes);
        if (const auto found = definitions_.find(std::string(name));
            found != definitions_.end()) {
            if (found->second != definition)
                raise(Runtime::Errors::Driver::Module::Definition::Conflicting, name);
            return;
        }
        Library library = Library::open(name, bytes);
        using Function = const Sdk::Driver::Module *(*)();
        void *raw = library.symbol("mln_runtime_driver");
        if (!raw)
            raise(Runtime::Errors::Driver::Module::Symbol::Missing, name, Library::error());
        const Sdk::Driver::Module *module = reinterpret_cast<Function>(raw)();
        if (!module)
            raise(Runtime::Errors::Driver::Module::Null, name);
        if (module->struct_size < sizeof(Sdk::Driver::Module))
            raise(Runtime::Errors::Driver::Module::Abi::Incompatible, name,
                  module->abi_version, Sdk::Driver::AbiVersion);
        if (!module->name)
            raise(Runtime::Errors::Driver::Module::Name::Missing, name);
        if (module->abi_version != Sdk::Driver::AbiVersion)
            raise(Runtime::Errors::Driver::Module::Abi::Incompatible, module->name,
                  module->abi_version, Sdk::Driver::AbiVersion);
        if (!module->format || !module->load || !module->destroy || !module->function_count ||
            !module->function || !module->invoke)
            raise(Runtime::Errors::Driver::Module::Register::Failed, module->name,
                  "driver module has an incomplete function table");
        if (module->option_count > 0 && !module->options)
            raise(Runtime::Errors::Driver::Module::Register::Failed, module->name,
                  "driver configuration schema is missing");

        std::vector<Sdk::Driver::Setting> settings;
        const auto configured = configuration.backends.find(module->name);
        const Configuration::Backend *backend =
            configured == configuration.backends.end() ? nullptr : &configured->second;
        std::set<std::string, std::less<>> accepted;
        for (std::size_t index = 0; index < module->option_count; ++index) {
            const Sdk::Driver::Option &option = module->options[index];
            if (!option.name || !*option.name || !accepted.emplace(option.name).second)
                raise(Runtime::Errors::Driver::Module::Register::Failed, module->name,
                      "driver configuration schema is invalid");
            if (!backend) {
                if (option.required)
                    raise(Runtime::Errors::Driver::Module::Register::Failed, module->name,
                          std::format("required backend option '{}' is missing", option.name));
                continue;
            }
            const auto value = backend->options.find(option.name);
            if (value == backend->options.end()) {
                if (option.required)
                    raise(Runtime::Errors::Driver::Module::Register::Failed, module->name,
                          std::format("required backend option '{}' is missing", option.name));
                continue;
            }
            Sdk::Driver::Setting setting{
                .name = option.name,
                .type = option.type,
                .boolean = false,
                .integer = 0,
                .number = 0.0,
                .string = nullptr,
            };
            bool matches = false;
            switch (option.type) {
                case Sdk::Driver::Value::Boolean:
                    if (const auto *v = std::get_if<bool>(&value->second)) {
                        setting.boolean = *v;
                        matches = true;
                    }
                    break;
                case Sdk::Driver::Value::Integer:
                    if (const auto *v = std::get_if<std::int64_t>(&value->second)) {
                        setting.integer = *v;
                        matches = true;
                    }
                    break;
                case Sdk::Driver::Value::Number:
                    if (const auto *v = std::get_if<double>(&value->second)) {
                        setting.number = *v;
                        matches = true;
                    }
                    break;
                case Sdk::Driver::Value::String:
                    if (const auto *v = std::get_if<std::string>(&value->second)) {
                        setting.string = v->c_str();
                        matches = true;
                    }
                    break;
            }
            if (!matches)
                raise(Runtime::Errors::Driver::Module::Register::Failed, module->name,
                      std::format("backend option '{}' has the wrong type", option.name));
            settings.push_back(setting);
        }
        if (backend)
            for (const auto &[key, value] : backend->options) {
                (void)value;
                if (!accepted.contains(key))
                    raise(Runtime::Errors::Configuration::Key::Unknown, key,
                          std::format("backend.{}", module->name));
            }
        if (!settings.empty() && !module->configure)
            raise(Runtime::Errors::Driver::Module::Register::Failed, module->name,
                  "driver does not accept configuration");
        if (module->configure) {
            std::array<char, 1024> error{};
            if (module->configure(settings.data(), settings.size(), error.data(), error.size()) != 0)
                raise(Runtime::Errors::Driver::Module::Register::Failed, module->name,
                      error.data());
        }
        factories_.emplace_back(*module, *registry_);
        definitions_.emplace(name, definition);
        libraries_.push_back(std::move(library));
    }
} // namespace Mln::Driver
