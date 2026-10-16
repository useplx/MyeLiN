#pragma once

#include "mln/error.hpp"
#include "mln/sdk/errors/sdk.hpp"
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    enum class SortDirection { Ascending, Descending };

    class Sort : public Operation::Of<Kind::Sort> {
        public:
            std::vector<Tensor> inputs;
            int64_t dimension;
            bool stable;
            SortDirection direction;

            Sort(std::vector<Tensor> values, int64_t axis, bool is_stable,
                 SortDirection order = SortDirection::Ascending)
                : inputs(std::move(values)), dimension(axis), stable(is_stable), direction(order) {
                if (inputs.empty())
                    Mln::raise(Errors::Operation::Operand::Missing, "sort");
            }

            std::vector<int64_t> signature() const override { return inputs.front().shape; }

            std::vector<std::vector<int64_t>> signatures() const override {
                std::vector<std::vector<int64_t>> shapes;
                shapes.reserve(inputs.size());
                for (const Tensor &input : inputs)
                    shapes.push_back(input.shape);
                return shapes;
            }
    };
}
