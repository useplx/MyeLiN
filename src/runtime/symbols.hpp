#pragma once

#include "vm.hpp"

#include <vector>

namespace Mln {
    class Runtime;
    namespace Wasm {
        std::vector<Symbol> core_symbols(Runtime &runtime);
    }
} // namespace Mln
