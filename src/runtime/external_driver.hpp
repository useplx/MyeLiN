#pragma once

#include "mln/sdk/driver/module.hpp"

#include <memory>
#include <string_view>

typedef struct iree_hal_driver_registry_t iree_hal_driver_registry_t;

namespace Mln::Driver {
    class Factory {
        public:
            Factory(const Sdk::Driver::Module &module,
                    iree_hal_driver_registry_t &registry);
            ~Factory();
            Factory(Factory &&) noexcept;
            Factory &operator=(Factory &&) noexcept;
            Factory(const Factory &) = delete;
            Factory &operator=(const Factory &) = delete;

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;
    };
} // namespace Mln::Driver
