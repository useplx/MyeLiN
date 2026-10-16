#include "mln/error.hpp"
#include "mln/sdk/errors/sdk.hpp"
#include <mln/sdk/module.hpp>
#include <mln/sdk/modules.hpp>

#include <format>
#include <stdexcept>

namespace Mln::Sdk {
    Modules::Modules(Module *parent_value) : parent(parent_value) {}

    Modules::~Modules() = default;

    Module *Modules::at(std::string_view name) {
        const auto found = modules.find(name);
        if (found == modules.end())
            raise(Sdk::Errors::Module::Missing, name);
        return found->second.value.get();
    }

    Module *Modules::at_typed(std::string_view name, const void *type) {
        Module *child = at(name);
        const auto found = modules.find(name);
        if (found == modules.end() || found->second.type != type) {
#if defined(__wasm__)
            __builtin_trap();
#else
            raise(Sdk::Errors::Module::Type::Mismatch, name);
#endif
        }
        return child;
    }

    void Modules::erase(std::string_view name) {
        if (const auto found = modules.find(name); found != modules.end())
            modules.erase(found);
    }

    void Modules::clear() { modules.clear(); }

    void Modules::insert(std::string name, std::unique_ptr<Module> child) {
        insert_typed(std::move(name), std::move(child), nullptr);
    }

    void Modules::insert_typed(std::string name, std::unique_ptr<Module> child, const void *type) {
        child->parent = parent;
        child->name = name;
        if (parent)
            child->bind_hook(parent->hook_);
        modules.emplace(std::move(name), Entry{std::move(child), type});
    }
}
