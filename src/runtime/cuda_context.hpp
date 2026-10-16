#pragma once

#include <cstddef>

namespace Mln::Cuda {
    using Context = void *;

    [[nodiscard]] Context current();

    class Guard {
        public:
            explicit Guard(Context context);
            ~Guard() noexcept;

            Guard(const Guard &) = delete;
            Guard &operator=(const Guard &) = delete;

    };
} // namespace Mln::Cuda
