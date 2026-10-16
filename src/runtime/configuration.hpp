#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace Mln::Configuration {
    enum class Scheduler { Static, Adaptive, Latency, Throughput };

    struct Backend {
            using Value = std::variant<bool, std::int64_t, double, std::string>;
            std::string device;
            int ordinal = 0;
            std::optional<std::size_t> threads;
            std::map<std::string, Value, std::less<>> options;
    };

    struct Runtime {
            struct Profile {
                    std::string output;
                    bool device_timing = false;
            };
            std::optional<std::size_t> memory;
            Scheduler scheduler = Scheduler::Static;
            bool profiling = false;
            std::optional<Profile> profile;
            std::size_t variants = 16;
            std::map<std::string, Backend, std::less<>> backends;
    };

    Runtime load(const std::filesystem::path &path);
    std::optional<std::filesystem::path> discover();
} // namespace Mln::Configuration
