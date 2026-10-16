#pragma once
#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    class Gather : public Operation::Of<Kind::Gather> {
        public:
            Tensor table;
            Tensor indices;
            int64_t axis;

            Gather(Tensor source, Tensor index_values, int64_t dimension)
                : table(std::move(source)), indices(std::move(index_values)), axis(dimension) {}

            std::vector<int64_t> signature() const override {
                std::vector<int64_t> shape;
                for (int64_t i = 0; i < axis; i++)
                    shape.push_back(table.shape[i]);
                for (int64_t d : indices.shape)
                    shape.push_back(d);
                for (std::size_t i = static_cast<std::size_t>(axis) + 1; i < table.shape.size();
                     i++)
                    shape.push_back(table.shape[i]);
                return shape;
            }
    };
}
