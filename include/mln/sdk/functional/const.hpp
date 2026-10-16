#pragma once
#include "mln/dtype.hpp"
#include "mln/sdk/operation.hpp"
#include "mln/sdk/weight.hpp"

#include <optional>

namespace Mln::Sdk::Functional {
    class Const : public Operation::Of<Kind::Const> {
        public:
            Weight weight;

            bool quantize;

            std::optional<DType> dtype;

            explicit Const(Weight value, bool should_quantize = false,
                           std::optional<DType> element_type = std::nullopt)
                : weight(std::move(value)), quantize(should_quantize), dtype(element_type) {}

            std::vector<int64_t> signature() const override { return weight.shape; }
    };
}
