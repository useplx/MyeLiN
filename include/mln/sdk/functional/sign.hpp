#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class Sign : public Unary<Kind::Sign> {
        public:
            explicit Sign(Tensor input) : Unary<Kind::Sign>(std::move(input)) {}
    };
}
