#pragma once

#include "mln/sdk/native/abi.hpp"
#include "library.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Mln::Native {
    class Registry {
        public:
            Registry() = default;
            Registry(const Registry &) = delete;
            Registry &operator=(const Registry &) = delete;
            Registry(Registry &&) noexcept = default;
            Registry &operator=(Registry &&) noexcept = default;
            ~Registry() = default;

            void load(std::string_view path);
            void load_memory(std::string_view name, std::span<const std::uint8_t> bytes);
            [[nodiscard]] const Sdk::Native::Symbol *resolve(std::string_view module,
                                                             std::string_view symbol) const;

        private:
            void register_provider(Library library, std::string_view origin);

            std::vector<Library> libraries_;
            std::unordered_map<std::string, Sdk::Native::Symbol> symbols_;
    };
} // namespace Mln::Native
