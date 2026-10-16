#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Select : public Operation::Of<Kind::Select> {
        public:
            Tensor condition;
            Tensor when_true;
            Tensor when_false;

            Select(Tensor predicate, Tensor true_value, Tensor false_value)
                : condition(std::move(predicate)), when_true(std::move(true_value)),
                  when_false(std::move(false_value)) {}

            std::vector<int64_t> signature() const override { return when_true.shape; }
    };
}
