#pragma once

#include "mln/sdk/operation.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace Mln::Sdk {
    class Operation::Compatibility {
        public:
            struct Target {
                    std::string_view name;
                    std::string_view device;
                    std::span<const std::pair<std::string, std::string>> properties;

                    [[nodiscard]] std::optional<std::string_view>
                    property(std::string_view key) const {
                        for (const auto &[property_name, value] : properties)
                            if (property_name == key)
                                return value;
                        return std::nullopt;
                    }
            };

            struct Context {
                    const Operation &operation;
                    std::span<const Operation *const> consumers;
                    std::optional<Target> target;
            };

            virtual bool applies(const Context &context) const = 0;

            virtual bool supports(const Context &context) const { return applies(context); }
            virtual std::optional<bool> expand(const Context &) const { return std::nullopt; }
            virtual bool barrier(const Context &) const { return false; }
            virtual std::optional<bool> fuse_producer(const Context &) const {
                return std::nullopt;
            }
            virtual std::optional<bool> linearize_reduction(const Context &) const {
                return std::nullopt;
            }
            virtual bool unroll(const Context &) const { return false; }
            virtual std::optional<int64_t> contraction_tile(const Context &) const {
                return std::nullopt;
            }
            virtual int64_t tile(const Context &, int64_t, int64_t extent) const { return extent; }
            virtual int priority() const { return 0; }
            virtual ~Compatibility() = default;
    };
}
