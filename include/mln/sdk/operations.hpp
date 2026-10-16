#pragma once

#include "mln/sdk/operation.hpp"
#include "mln/sdk/tensor.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace Mln::Sdk {
    class Module;

    class Operations {
        private:
            std::vector<std::shared_ptr<Operation>> operations;
            Module *owner = nullptr;
            std::string fallback_path;

            std::string current_policy_path() const;

        public:
            Operations() = default;
            explicit Operations(Module &owner);

            void set_policy_path(std::string path) {
                fallback_path = std::move(path);
                for (const auto &operation : operations) {
                    if (operation->policy_path.empty())
                        operation->policy_path = fallback_path;
                }
            }

            template <typename T, typename... Args> Tensor insert(Args &&...args) {
                auto op = std::make_shared<T>(std::forward<Args>(args)...);
                op->policy_path = current_policy_path();
                if constexpr (requires(T &value, std::string path) {
                                  value.set_policy_path(std::move(path));
                              }) {
                    op->set_policy_path(op->policy_path);
                }

                Tensor result{};
                result.def = op;
                result.shape = op->signature();

                operations.push_back(std::move(op));
                return result;
            }

            template <typename T, typename... Args>
            std::vector<Tensor> insert_many(Args &&...args) {
                auto op = std::make_shared<T>(std::forward<Args>(args)...);
                op->policy_path = current_policy_path();
                if constexpr (requires(T &value, std::string path) {
                                  value.set_policy_path(std::move(path));
                              }) {
                    op->set_policy_path(op->policy_path);
                }

                const auto shapes = op->signatures();
                const auto dtypes = op->result_dtypes();
                std::vector<Tensor> results;
                results.reserve(shapes.size());
                for (std::size_t i = 0; i < shapes.size(); ++i) {
                    Tensor result{};
                    result.def = op;
                    result.shape = shapes[i];
                    result.result = i;
                    if (i < dtypes.size())
                        result.dtype = dtypes[i];
                    results.push_back(std::move(result));
                }

                operations.push_back(std::move(op));
                return results;
            }

            auto begin() { return operations.begin(); }
            auto end() { return operations.end(); }

            auto begin() const { return operations.begin(); }
            auto end() const { return operations.end(); }

            void clear() noexcept { operations.clear(); }
    };
}
