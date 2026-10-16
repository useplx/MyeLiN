#pragma once

#include "mln/sdk/operation.hpp"

#include <optional>
#include <utility>

namespace Mln::Sdk::Functional {

    class Attention : public Operation::Of<Kind::Attention> {
        public:
            Tensor query;
            Tensor key;
            Tensor value;
            double scale;
            std::optional<Tensor> mask;

            Attention(Tensor query_tensor, Tensor key_tensor, Tensor value_tensor,
                      double scale_value)
                : query(std::move(query_tensor)), key(std::move(key_tensor)),
                  value(std::move(value_tensor)), scale(scale_value) {}

            Attention(Tensor query_tensor, Tensor key_tensor, Tensor value_tensor,
                      double scale_value, Tensor mask_tensor)
                : query(std::move(query_tensor)), key(std::move(key_tensor)),
                  value(std::move(value_tensor)), scale(scale_value), mask(std::move(mask_tensor)) {
            }

            std::vector<int64_t> signature() const override {
                auto shape = query.shape;
                if (!shape.empty() && !value.shape.empty())
                    shape.back() = value.shape.back();
                return shape;
            }
    };
}
