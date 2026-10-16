#pragma once
#include "mln/sdk/module.hpp"
#include "mln/sdk/tensor.hpp"

namespace Mln::Sdk::Nn {
    class SiLU : public Module {
        public:
            SiLU() = default;
            Tensor forward(const Tensor &x);
    };
}
