#pragma once

#include "mln/sdk/operation.hpp"

namespace Mln::Sdk::Functional {
    enum class ScatterUpdate { Replace, Add, Multiply, Maximum, Minimum };

    class Scatter : public Operation::Of<Kind::Scatter> {
        public:
            Tensor input;
            Tensor indices;
            Tensor updates;
            std::vector<int64_t> update_window_dimensions;
            std::vector<int64_t> inserted_window_dimensions;
            std::vector<int64_t> input_batching_dimensions;
            std::vector<int64_t> indices_batching_dimensions;
            std::vector<int64_t> scatter_to_operand_dimensions;
            int64_t index_vector_dimension;
            bool indices_are_sorted;
            bool unique_indices;
            ScatterUpdate update;

            Scatter(Tensor operand, Tensor scatter_indices, Tensor update_values,
                    std::vector<int64_t> update_window_dims,
                    std::vector<int64_t> inserted_window_dims,
                    std::vector<int64_t> input_batching_dims,
                    std::vector<int64_t> scatter_indices_batching_dims,
                    std::vector<int64_t> scatter_dims_to_operand_dims, int64_t index_vector_dim,
                    bool sorted, bool unique, ScatterUpdate update_operation)
                : input(std::move(operand)), indices(std::move(scatter_indices)),
                  updates(std::move(update_values)),
                  update_window_dimensions(std::move(update_window_dims)),
                  inserted_window_dimensions(std::move(inserted_window_dims)),
                  input_batching_dimensions(std::move(input_batching_dims)),
                  indices_batching_dimensions(std::move(scatter_indices_batching_dims)),
                  scatter_to_operand_dimensions(std::move(scatter_dims_to_operand_dims)),
                  index_vector_dimension(index_vector_dim), indices_are_sorted(sorted),
                  unique_indices(unique), update(update_operation) {}

            std::vector<int64_t> signature() const override { return input.shape; }
    };
}
