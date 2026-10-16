#pragma once
#include "mln/sdk/functional/unary.hpp"

namespace Mln::Sdk::Functional {
    class Log : public Unary<Kind::Log> {
        public:
            explicit Log(Tensor input) : Unary<Kind::Log>(std::move(input)) {}
    };
}
