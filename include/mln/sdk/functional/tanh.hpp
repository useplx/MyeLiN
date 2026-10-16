#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Tanh : public Unary<Kind::Tanh> {
        public:
            explicit Tanh(Tensor input) : Unary<Kind::Tanh>(std::move(input)) {}
    };
}
