#pragma once
#include "mln/sdk/module.hpp"
#include "mln/sdk/tensor.hpp"

namespace Mln::Sdk::Nn {
    class GELU : public Module {
        public:
            GELU() = default;
            Tensor forward(const Tensor &x);
    };
}
