#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Negate : public Unary<Kind::Negate> {
        public:
            explicit Negate(Tensor input) : Unary<Kind::Negate>(std::move(input)) {}
    };
}
