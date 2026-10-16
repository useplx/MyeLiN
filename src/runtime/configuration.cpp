#include "configuration.hpp"

#include "mln/error.hpp"
#include "mln/runtime/errors.hpp"

#include <toml++/toml.hpp>

#include <array>
#include <charconv>
#include <limits>
#include <set>
#include <system_error>
#include <type_traits>

namespace Mln::Configuration {
    namespace {
        using Keys = std::set<std::string, std::less<>>;

        struct Registration {
                std::string_view name;
                Keys keys;
                bool threads = false;
        };

        const std::array registrations{
            Registration{"local", {"device", "ordinal", "threads"}, true},
            Registration{"cuda", {"device", "ordinal"}},
            Registration{"vulkan", {"device", "ordinal"}},
            Registration{"hip", {"device", "ordinal"}},
            Registration{"metal", {"device", "ordinal"}},
        };

        const toml::table *optional_table(const toml::table &parent, std::string_view key,
                                          std::string_view location) {
            const toml::node *node = parent.get(key);
            if (!node)
                return nullptr;
            const toml::table *table = node->as_table();
            if (!table)
                raise(Mln::Runtime::Errors::Configuration::Type::Invalid, location);
            return table;
        }

        template <typename T>
        std::optional<T> optional_value(const toml::table &table, std::string_view key,
                                        std::string_view location) {
            const toml::node *node = table.get(key);
            if (!node)
                return std::nullopt;
            const auto value = node->value<T>();
            if (!value)
                raise(Mln::Runtime::Errors::Configuration::Type::Invalid, location);
            return *value;
        }

        Backend::Value backend_value(const toml::node &node, std::string_view location) {
            if (const auto value = node.value<bool>())
                return *value;
            if (const auto value = node.value<std::int64_t>())
                return *value;
            if (const auto value = node.value<double>())
                return *value;
            if (const auto value = node.value<std::string>())
                return *value;
            raise(Mln::Runtime::Errors::Configuration::Type::Invalid, location);
        }

        std::size_t memory(std::string_view input) {
            if (input.empty())
                raise(Mln::Runtime::Errors::Configuration::Memory::Invalid, input);
            std::size_t split = 0;
            while (split < input.size() && input[split] >= '0' && input[split] <= '9')
                ++split;
            std::uint64_t value = 0;
            const auto parsed = std::from_chars(input.data(), input.data() + split, value);
            if (parsed.ec != std::errc{} || parsed.ptr != input.data() + split)
                raise(Mln::Runtime::Errors::Configuration::Memory::Invalid, input);
            const std::string_view suffix = input.substr(split);
            std::uint64_t multiplier = 1;
            if (suffix == "KiB")
                multiplier = 1024ULL;
            else if (suffix == "MiB")
                multiplier = 1024ULL * 1024ULL;
            else if (suffix == "GiB")
                multiplier = 1024ULL * 1024ULL * 1024ULL;
            else if (!suffix.empty() && suffix != "B")
                raise(Mln::Runtime::Errors::Configuration::Memory::Invalid, input);
            if (value > std::numeric_limits<std::size_t>::max() / multiplier)
                raise(Mln::Runtime::Errors::Configuration::Memory::Invalid, input);
            return static_cast<std::size_t>(value * multiplier);
        }

        Scheduler scheduler(std::string_view value) {
            if (value == "static")
                return Scheduler::Static;
            if (value == "adaptive")
                return Scheduler::Adaptive;
            if (value == "latency")
                return Scheduler::Latency;
            if (value == "throughput")
                return Scheduler::Throughput;
            raise(Mln::Runtime::Errors::Configuration::Scheduler::Invalid, value);
        }

        void validate_keys(const toml::table &table, const Keys &keys,
                           std::string_view location) {
            for (const auto &[key, value] : table) {
                (void)value;
                if (!keys.contains(std::string(key.str())))
                    raise(Mln::Runtime::Errors::Configuration::Key::Unknown, key.str(), location);
            }
        }
    } // namespace

    std::optional<std::filesystem::path> discover() {
        const std::filesystem::path candidate = std::filesystem::current_path() / "myelin.toml";
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error)
            return candidate;
        return std::nullopt;
    }

    Runtime load(const std::filesystem::path &path) {
        toml::table document;
        try {
            document = toml::parse_file(path.string());
        } catch (const toml::parse_error &error) {
            raise(Mln::Runtime::Errors::Configuration::Parse::Failed, path.string(),
                  error.description());
        }

        validate_keys(document, {"runtime", "backend"}, "root");
        Runtime result;
        if (const toml::table *runtime = optional_table(document, "runtime", "runtime")) {
            validate_keys(*runtime, {"memory", "scheduler", "profiling", "variants", "profile"}, "runtime");
            if (const auto value = optional_value<std::string>(*runtime, "memory", "runtime.memory"))
                result.memory = memory(*value);
            if (const auto value = optional_value<std::string>(*runtime, "scheduler",
                                                               "runtime.scheduler"))
                result.scheduler = scheduler(*value);
            if (const auto value = optional_value<bool>(*runtime, "profiling",
                                                        "runtime.profiling"))
                result.profiling = *value;
            if (const auto value = optional_value<std::int64_t>(*runtime, "variants",
                                                                "runtime.variants")) {
                if (*value <= 0 || static_cast<std::uint64_t>(*value) >
                                       std::numeric_limits<std::size_t>::max())
                    raise(Mln::Runtime::Errors::Configuration::Type::Invalid,
                          "runtime.variants");
                result.variants = static_cast<std::size_t>(*value);
            }
            if (const toml::table *profile = optional_table(*runtime, "profile", "runtime.profile")) {
                validate_keys(*profile, {"output", "device_timing"}, "runtime.profile");
                Configuration::Runtime::Profile value;
                value.output = optional_value<std::string>(*profile, "output",
                                                           "runtime.profile.output")
                                   .value_or("");
                if (value.output.empty())
                    raise(Mln::Runtime::Errors::Configuration::Type::Invalid,
                          "runtime.profile.output");
                value.device_timing = optional_value<bool>(*profile, "device_timing",
                                                            "runtime.profile.device_timing")
                                          .value_or(false);
                result.profile = std::move(value);
            }
        }

        if (const toml::table *backends = optional_table(document, "backend", "backend")) {
            for (const auto &[name, node] : *backends) {
                const toml::table *table = node.as_table();
                if (!table)
                    raise(Mln::Runtime::Errors::Configuration::Backend::Invalid, name.str());
                const Registration *registered = nullptr;
                for (const Registration &candidate : registrations)
                    if (candidate.name == name.str()) {
                        registered = &candidate;
                        break;
                    }
                const std::string location = std::string("backend.") + std::string(name.str());
                if (registered)
                    validate_keys(*table, registered->keys, location);
                Backend backend;
                backend.device = optional_value<std::string>(*table, "device", location + ".device")
                                     .value_or(std::string(name.str()));
                const std::int64_t ordinal =
                    optional_value<std::int64_t>(*table, "ordinal", location + ".ordinal")
                        .value_or(0);
                if (ordinal < 0 || ordinal > std::numeric_limits<int>::max())
                    raise(Mln::Runtime::Errors::Configuration::Backend::Ordinal::Invalid,
                          name.str());
                backend.ordinal = static_cast<int>(ordinal);
                if (const auto threads =
                        optional_value<std::int64_t>(*table, "threads", location + ".threads")) {
                    if (!registered || !registered->threads || *threads <= 0 ||
                        static_cast<std::uint64_t>(*threads) >
                            std::numeric_limits<std::size_t>::max())
                        raise(Mln::Runtime::Errors::Configuration::Backend::Threads::Invalid,
                              name.str());
                    backend.threads = static_cast<std::size_t>(*threads);
                }
                if (!registered) {
                    for (const auto &[key, value] : *table) {
                        const std::string option(key.str());
                        if (option == "device" || option == "ordinal")
                            continue;
                        backend.options.emplace(
                            option, backend_value(value, location + "." + option));
                    }
                }
                result.backends.emplace(name.str(), std::move(backend));
            }
        }
        return result;
    }
} // namespace Mln::Configuration
