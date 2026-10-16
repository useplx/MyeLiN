
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "mln/dtype.hpp"

namespace Mln::Sdk {
    class Operation;

    class Tensor {
        public:
            static constexpr int64_t Dynamic = -1;

            std::vector<int64_t> shape;
            std::optional<DType> dtype;
            std::shared_ptr<Operation> def;
            std::size_t result = 0;

            Tensor() = default;
            explicit Tensor(std::vector<int64_t> shape);
            Tensor(std::vector<int64_t> shape, DType dtype);
    };
}
