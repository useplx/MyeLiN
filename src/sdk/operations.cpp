#include <mln/sdk/operations.hpp>

#include <mln/sdk/module.hpp>

namespace Mln::Sdk {
    Operations::Operations(Module &module) : owner(&module) {}

    std::string Operations::current_policy_path() const {
        return owner ? owner->path() : fallback_path;
    }
}
