#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Ceil : public Unary<Kind::Ceil> {
        public:
            explicit Ceil(Tensor input) : Unary<Kind::Ceil>(std::move(input)) {}
    };
}
