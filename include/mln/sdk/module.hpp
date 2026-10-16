#pragma once

#include <format>
#include <string>

#include "mln/sdk/hook.hpp"
#include "mln/sdk/modules.hpp"
#include "mln/sdk/operations.hpp"
#include "mln/sdk/parameters.hpp"

#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Mln::Sdk {
    class Module {
        private:
            friend class Modules;
            Graph::Hook *hook_ = nullptr;

        public:
            Module *parent = nullptr;
            std::string name;
            Modules modules;
            Parameters parameters;
            Operations operations;
            Module();
            virtual ~Module();
            std::string path() const;
            virtual std::string entrypoint() const;
            void bind_hook(Graph::Hook *hook);

            virtual std::vector<Tensor> graph(std::span<const Tensor> inputs);
            virtual std::vector<std::vector<Tensor>> specializations() const;

            Tensor apply(Tensor output, std::span<const Tensor> inputs = {},
                         std::string_view name = {});
    };

    namespace Graph::Invocation {
        template <typename T> struct is_tensor_tuple : std::false_type {};

        template <typename... T>
        struct is_tensor_tuple<std::tuple<T...>>
            : std::bool_constant<(std::is_same_v<T, Tensor> && ...)> {};

        inline void append_hook_inputs(std::vector<Tensor> &inputs, const Tensor &tensor) {
            inputs.push_back(tensor);
        }

        inline void append_hook_inputs(std::vector<Tensor> &inputs,
                                       const std::vector<Tensor> &tensors) {
            inputs.insert(inputs.end(), tensors.begin(), tensors.end());
        }

        template <typename T> void append_hook_inputs(std::vector<Tensor> &, const T &) {}

        template <typename Result>
        void apply_hook(Module &module, std::span<const Tensor> inputs, Result &result) {
            if constexpr (std::is_same_v<std::remove_cvref_t<Result>, Tensor>) {
                result = module.apply(std::move(result), inputs);
            } else if constexpr (is_tensor_tuple<std::remove_cvref_t<Result>>::value) {
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    ((std::get<I>(result) = module.apply(std::move(std::get<I>(result)), inputs,
                                                         std::format("{}", I))),
                     ...);
                }(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<Result>>>{});
            } else if constexpr (std::is_same_v<std::remove_cvref_t<Result>, std::vector<Tensor>>) {
                for (std::size_t i = 0; i < result.size(); ++i)
                    result[i] = module.apply(std::move(result[i]), inputs, std::format("{}", i));
            } else if constexpr (requires(Result &value) { value.apply_hook(module, inputs); }) {
                result.apply_hook(module, inputs);
            } else {
                static_assert(std::is_same_v<Result, void>,
                              "Module::forward must return Tensor or "
                              "implement apply_hook(Module&, span<const Tensor>)");
            }
        }
    }

    template <typename T, typename... Args>
    auto Modules::forward(std::string_view name, Args &&...args) {
        T *module = at<T>(name);
        std::vector<Tensor> inputs;
        inputs.reserve(sizeof...(Args));
        (Graph::Invocation::append_hook_inputs(inputs, args), ...);
        auto result = module->forward(std::forward<Args>(args)...);
        Graph::Invocation::apply_hook(*module, inputs, result);

        return result;
    }
}
