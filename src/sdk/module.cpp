#include "mln/error.hpp"
#include "mln/sdk/errors/sdk.hpp"
#include <mln/sdk/module.hpp>

#include <stdexcept>

namespace Mln::Sdk {
    Module::Module() : modules(this), parameters(*this), operations(*this) {}

    Module::~Module() = default;

    std::vector<Tensor> Module::graph(std::span<const Tensor>) {
#if defined(__wasm__)
        __builtin_trap();
#else
        raise(Sdk::Errors::Module::Graph::Unimplemented);
#endif
    }

    std::vector<std::vector<Tensor>> Module::specializations() const { return {}; }

    std::string Module::path() const {
        if (parent)
            return parent->path() + "." + name;
        return name;
    }

    std::string Module::entrypoint() const { return path() + ".forward"; }

    void Module::bind_hook(Graph::Hook *hook) {
        hook_ = hook;
        for (auto &[child_name, child] : modules) {
            (void)child_name;
            child.value->bind_hook(hook);
        }
    }

    Tensor Module::apply(Tensor output, std::span<const Tensor> inputs,
                         std::string_view operation_name) {
        if (!hook_)
            return output;
        std::string output_path = path();
        if (!operation_name.empty()) {
            output_path.push_back('.');
            output_path.append(operation_name);
        }

        return hook_->apply(output_path, *this, inputs, std::move(output));
    }
}
