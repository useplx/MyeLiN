#include "mln/error.hpp"
#include "mln/sdk/errors/sdk.hpp"
#include <mln/sdk/orchestration.hpp>
#include <mln/sdk/orchestrations.hpp>

#include <format>
#include <stdexcept>

namespace Mln::Sdk {
    Orchestration *Orchestrations::at(std::string_view name) {
        const auto found = orchestrations.find(name);
        if (found == orchestrations.end())
            raise(Sdk::Errors::Orchestration::Missing, name);
        return found->second.value.get();
    }

    Orchestration *Orchestrations::at_typed(std::string_view name, const void *type) {
        Orchestration *child = at(name);
        const auto found = orchestrations.find(name);
        if (found == orchestrations.end() || found->second.type != type) {
#if defined(__wasm__)
            __builtin_trap();
#else
            raise(Sdk::Errors::Orchestration::Type::Mismatch, name);
#endif
        }
        return child;
    }

    void Orchestrations::erase(std::string_view name) {
        if (const auto found = orchestrations.find(name); found != orchestrations.end())
            orchestrations.erase(found);
    }

    void Orchestrations::clear() { orchestrations.clear(); }

    void Orchestrations::insert(std::string name, std::unique_ptr<Orchestration> child) {
        insert_typed(std::move(name), std::move(child), nullptr);
    }

    void Orchestrations::insert_typed(std::string name, std::unique_ptr<Orchestration> child,
                                      const void *type) {
        orchestrations.emplace(std::move(name), Entry{std::move(child), type});
    }
}
