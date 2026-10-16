#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Sin : public Unary<Kind::Sin> {
        public:
            explicit Sin(Tensor input) : Unary<Kind::Sin>(std::move(input)) {}
    };
}
