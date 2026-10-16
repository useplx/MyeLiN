#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Exp : public Unary<Kind::Exp> {
        public:
            explicit Exp(Tensor input) : Unary<Kind::Exp>(std::move(input)) {}
    };
}
