#pragma once

#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class IsFinite : public Unary<Kind::IsFinite> {
        public:
            explicit IsFinite(Tensor input) : Unary<Kind::IsFinite>(std::move(input)) {}
    };
}
