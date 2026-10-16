#pragma once

#include <cstdint>

#include "mln/sdk/module.hpp"
#include "mln/sdk/tensor.hpp"
#include "mln/sdk/weight.hpp"

namespace Mln::Sdk::Nn {
    class Linear : public Module {
        public:
            Linear(Weight weight, Weight bias);
            Tensor forward(const Tensor &x);
    };
}
