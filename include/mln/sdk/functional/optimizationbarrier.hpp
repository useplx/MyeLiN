#pragma once

#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class OptimizationBarrier : public Unary<Kind::OptimizationBarrier> {
        public:
            explicit OptimizationBarrier(Tensor input)
                : Unary<Kind::OptimizationBarrier>(std::move(input)) {}
    };
}
