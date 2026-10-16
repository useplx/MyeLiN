#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Mln::Sdk {
    struct Weight {
            std::string name;
            std::vector<int64_t> shape;

            std::optional<double> value;
    };
}
