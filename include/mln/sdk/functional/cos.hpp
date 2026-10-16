#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Cos : public Unary<Kind::Cos> {
        public:
            explicit Cos(Tensor input) : Unary<Kind::Cos>(std::move(input)) {}
    };
}
