#pragma once

#include "mln/dtype.hpp"
#include "mln/sdk/functional/input.hpp"
#include "mln/sdk/operations.hpp"

#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace Mln::Sdk::Functional {
    class Map : public Operation::Of<Kind::Map> {
        public:
            struct Operand {
                    Tensor value;
                    DType dtype;
                    std::vector<int64_t> dimensions;
            };

            std::vector<Operand> operands;
            std::vector<int64_t> output_shape;
            DType output_dtype;
            std::unique_ptr<Operations> body;
            std::vector<Tensor> body_arguments;
            Tensor result;

            template <typename Build>
            Map(std::vector<Operand> mapped_operands, std::vector<int64_t> shape, DType dtype,
                Build &&build)
                : operands(std::move(mapped_operands)), output_shape(std::move(shape)),
                  output_dtype(dtype), body(std::make_unique<Operations>()) {
                body_arguments.reserve(operands.size());
                for (const auto &operand : operands) {
                    Tensor argument = body->insert<Input>();
                    argument.dtype = operand.dtype;
                    body_arguments.push_back(std::move(argument));
                }
                result = std::forward<Build>(build)(*body, std::span<const Tensor>(body_arguments));
            }

            std::vector<int64_t> signature() const override { return output_shape; }
    };
}
