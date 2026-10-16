#pragma once

#include "kind.hpp"
#include "tensor.hpp"
#include <string>
#include <vector>

namespace Mln::Sdk {
    class Operation {
        protected:
            explicit Operation(Functional::Kind operation_kind) : kind(operation_kind) {}

        public:
            class Compatibility;

            template <Functional::Kind K> class Of;

            const Functional::Kind kind;
            std::string policy_path;

            virtual ~Operation() = default;

            virtual std::vector<int64_t> signature() const = 0;

            virtual std::vector<std::vector<int64_t>> signatures() const { return {signature()}; }

            virtual std::vector<std::optional<DType>> result_dtypes() const { return {}; }
    };

    template <Functional::Kind K> class Operation::Of : public Operation {
        public:
            static constexpr Functional::Kind kind_value = K;

        protected:
            Of() : Operation(K) {}
    };
}
