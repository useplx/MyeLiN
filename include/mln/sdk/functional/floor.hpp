#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Floor : public Unary<Kind::Floor> {
        public:
            explicit Floor(Tensor input) : Unary<Kind::Floor>(std::move(input)) {}
    };
}
