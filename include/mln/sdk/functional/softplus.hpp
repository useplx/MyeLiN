#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Softplus : public Unary<Kind::Softplus> {
        public:
            explicit Softplus(Tensor input) : Unary<Kind::Softplus>(std::move(input)) {}
    };
}
