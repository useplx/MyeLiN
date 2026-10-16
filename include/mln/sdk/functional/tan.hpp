#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class Tan : public Unary<Kind::Tan> {
        public:
            explicit Tan(Tensor input) : Unary<Kind::Tan>(std::move(input)) {}
    };
}
