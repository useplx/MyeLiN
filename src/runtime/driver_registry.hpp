#pragma once

#include "library.hpp"
#include "external_driver.hpp"
#include "configuration.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

typedef struct iree_hal_driver_registry_t iree_hal_driver_registry_t;

namespace Mln::Driver {
    class Registry {
        public:
            Registry() noexcept = default;
            explicit Registry(iree_hal_driver_registry_t &registry) noexcept;
            void load(std::string_view name, std::span<const std::uint8_t> bytes,
                      const Configuration::Runtime &configuration);

        private:
            iree_hal_driver_registry_t *registry_ = nullptr;
            std::unordered_map<std::string, std::size_t> definitions_;
            std::vector<Library> libraries_;
            std::vector<Factory> factories_;
    };
} // namespace Mln::Driver
