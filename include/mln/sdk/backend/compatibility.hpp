#pragma once

#include "mln/sdk/operation/compatibility.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Mln::Sdk {
    struct Backend {
            class Compatibility;

            struct Resource {
                    std::string_view name;
                    std::string_view format;
                    std::span<const std::int64_t> shape;
                    std::span<const std::byte> data;
            };

            struct Compilation {
                    enum class Element { I4, U4, I8, I16, I32, F16, BF16, F32 };

                    struct Entrypoint {
                            std::string name;
                            std::uint32_t contractions = 0;
                    };

                    struct Binding {
                            std::string entrypoint;
                            std::uint32_t ordinal = 0;
                            std::uint64_t offset = 0;
                            std::uint64_t length = 0;
                            std::string_view resource;
                            std::vector<std::int64_t> shape;
                            Element element = Element::F32;
                            bool read = false;
                            bool written = false;
                    };

                    std::span<const Resource> resources;
                    std::span<const Entrypoint> entrypoints;
                    std::span<const Binding> bindings;
            };

            std::string device;
            int ordinal = 0;
            std::vector<std::pair<std::string, std::string>> flags;

            bool operator==(const Backend &) const = default;
    };

    class Backend::Compatibility : public Operation::Compatibility {
        public:
            enum class Legality { Unsupported, Exact, Approximate };

            virtual ~Compatibility() = default;

            [[nodiscard]] virtual std::string_view name() const noexcept = 0;
            [[nodiscard]] virtual std::string_view format() const noexcept = 0;

            [[nodiscard]] virtual Legality legality(const Context &) const {
                return Legality::Unsupported;
            }

            bool applies(const Context &context) const final {
                return context.target && context.target->device == name() &&
                       legality(context) != Legality::Unsupported;
            }

            [[nodiscard]] virtual std::expected<std::vector<std::byte>, std::string>
            serialize(const Compilation &compilation) const = 0;
    };
}
