#pragma once

#include "mln/sdk/tensor.hpp"

#include <span>
#include <string_view>

namespace Mln::Sdk {
    class Module;
    namespace Graph {

        class Hook {
            public:
                virtual ~Hook() = default;

                virtual Tensor apply(std::string_view path, Module &module,
                                     std::span<const Tensor> inputs, Tensor output) = 0;
        };
    }
}
