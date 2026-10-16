#pragma once
#include "mln/sdk/functional/unary.hpp"
namespace Mln::Sdk::Functional {
    class BitwiseNot : public Unary<Kind::BitwiseNot> {
        public:
            explicit BitwiseNot(Tensor input) : Unary<Kind::BitwiseNot>(std::move(input)) {}
    };
}
