#include "mln/runtime/metadata.hpp"
#include "mln/error.hpp"
#include "mln/runtime/errors.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace Mln {
    namespace {
        std::string_view as_text(std::span<const uint8_t> bytes) {
            return std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        }

        bool next_line(std::string_view &text, std::string_view &line) {
            if (text.empty())
                return false;
            const std::size_t newline = text.find('\n');
            line = text.substr(0, newline);
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            text =
                newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
            return true;
        }
    } // namespace

    std::string current_platform() {
#if defined(__linux__)
        constexpr std::string_view os = "linux";
#elif defined(_WIN32)
        constexpr std::string_view os = "windows";
#elif defined(__APPLE__)
        constexpr std::string_view os = "macos";
#else
        constexpr std::string_view os = "unknown";
#endif
#if defined(__x86_64__) || defined(_M_X64)
        constexpr std::string_view architecture = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
        constexpr std::string_view architecture = "aarch64";
#else
        constexpr std::string_view architecture = "unknown";
#endif
        return std::string(os) + "-" + std::string(architecture);
    }

    std::vector<uint8_t> encode_requirements(const Artifact::Requirements &requirements) {
        const std::string text = "version=1\nruntime_abi=" +
                                 std::to_string(requirements.runtime_abi) + "\nnative_abi=" +
                                 std::to_string(requirements.native_abi) + "\ndriver_abi=" +
                                 std::to_string(requirements.driver_abi) + "\nplatform=" +
                                 requirements.platform + "\n";
        return {text.begin(), text.end()};
    }

    Artifact::Requirements decode_requirements(std::span<const uint8_t> bytes) {
        Artifact::Requirements result;
        bool version = false, runtime = false, native = false, driver = false, platform = false;
        std::string_view text = as_text(bytes);
        for (std::string_view line; next_line(text, line);) {
            const std::size_t separator = line.find('=');
            if (separator == std::string_view::npos)
                throw std::runtime_error("invalid artifact requirements");
            const std::string_view key = line.substr(0, separator);
            const std::string_view value = line.substr(separator + 1);
            auto number = [&](uint32_t &destination) {
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                                                    destination);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                    throw std::runtime_error("invalid artifact requirement value");
            };
            if (key == "version") {
                uint32_t value_number = 0;
                number(value_number);
                if (value_number != 1) throw std::runtime_error("unsupported requirements version");
                version = true;
            } else if (key == "runtime_abi") {
                number(result.runtime_abi); runtime = true;
            } else if (key == "native_abi") {
                number(result.native_abi); native = true;
            } else if (key == "driver_abi") {
                number(result.driver_abi); driver = true;
            } else if (key == "platform") {
                result.platform.assign(value); platform = true;
            } else {
                throw std::runtime_error("unknown artifact requirement");
            }
        }
        if (!version || !runtime || !native || !driver || !platform)
            throw std::runtime_error("incomplete artifact requirements");
        return result;
    }

    std::vector<std::string> decode_devices(std::span<const uint8_t> bytes) {
        std::vector<std::string> devices;
        std::string_view text = as_text(bytes);
        for (std::string_view line; next_line(text, line);)
            if (!line.empty())
                devices.emplace_back(line);
        if (devices.empty())
            raise(Runtime::Errors::Metadata::Devices::Empty);
        return devices;
    }

    std::vector<Artifact::Entrypoint>
    decode_application_entrypoints(std::span<const uint8_t> bytes) {
        std::vector<Artifact::Entrypoint> result;
        std::string_view text = as_text(bytes);
        for (std::string_view line; next_line(text, line);) {
            const std::size_t first = line.find('\t');
            const std::size_t second = first == std::string_view::npos ? std::string_view::npos
                                                                       : line.find('\t', first + 1);
            const std::size_t third = second == std::string_view::npos ? std::string_view::npos
                                                                        : line.find('\t', second + 1);
            const bool legacy = third == std::string_view::npos;
            if (first == 0 || first == std::string_view::npos || second == first + 1 ||
                second == std::string_view::npos || second + 1 == line.size() ||
                (!legacy && (third == second + 1 || third + 1 == line.size() ||
                             line.find('\t', third + 1) != std::string_view::npos)))
                raise(Runtime::Errors::Metadata::Entrypoint::Invalid);

            Artifact::Runtime runtime = Artifact::Runtime::Wasm;
            if (!legacy) {
                const std::string_view domain = line.substr(first + 1, second - first - 1);
                if (domain == "graph") runtime = Artifact::Runtime::Graph;
                else if (domain == "native") runtime = Artifact::Runtime::Native;
                else if (domain == "wasm") runtime = Artifact::Runtime::Wasm;
                else raise(Runtime::Errors::Metadata::Entrypoint::Invalid);
            }

            Artifact::Entrypoint entrypoint{
                std::string(line.substr(0, first)),
                legacy ? std::string(line.substr(first + 1, second - first - 1))
                       : std::string(line.substr(second + 1, third - second - 1)),
                legacy ? std::string(line.substr(second + 1))
                       : std::string(line.substr(third + 1)),
                runtime,
            };
            const bool duplicated =
                std::any_of(result.begin(), result.end(), [&](const Artifact::Entrypoint &other) {
                    return other.name == entrypoint.name;
                });
            if (duplicated)
                raise(Runtime::Errors::Metadata::Entrypoint::Duplicated, entrypoint.name);
            result.push_back(std::move(entrypoint));
        }
        if (result.empty())
            raise(Runtime::Errors::Metadata::Entrypoint::None);
        return result;
    }

    std::vector<Artifact::Hook> decode_hooks(std::span<const uint8_t> bytes) {
        std::vector<Artifact::Hook> result;
        std::string_view text = as_text(bytes);
        for (std::string_view line; next_line(text, line);) {
            const std::size_t first = line.find('\t');
            const std::size_t second = first == std::string_view::npos ? std::string_view::npos
                                                                       : line.find('\t', first + 1);
            if (first == 0 || first == std::string_view::npos || second == first + 1 ||
                second == std::string_view::npos || second + 1 == line.size() ||
                line.find('\t', second + 1) != std::string_view::npos)
                raise(Runtime::Errors::Metadata::Hook::Invalid);
            result.push_back({std::string(line.substr(0, first)),
                              std::string(line.substr(first + 1, second - first - 1)),
                              std::string(line.substr(second + 1))});
        }
        return result;
    }

    std::vector<Artifact::Signature> decode_signatures(std::span<const uint8_t> bytes) {
        std::vector<Artifact::Signature> result;
        std::string_view text = as_text(bytes);
        for (std::string_view line; next_line(text, line);) {
            const std::size_t first_tab = line.find('\t');
            if (first_tab == std::string_view::npos || first_tab == 0)
                raise(Runtime::Errors::Metadata::Entrypoint::Signature::Invalid);
            Artifact::Signature signature;
            signature.path.assign(line.substr(0, first_tab));
            line.remove_prefix(first_tab + 1);
            if (line.empty() || line.front() != '@')
                raise(Runtime::Errors::Metadata::Specialization::Symbol::Missing);
            const std::size_t tab = line.find('\t');
            const std::string_view field = line.substr(1, tab - 1);
            if (field.empty() || tab == std::string_view::npos)
                raise(Runtime::Errors::Metadata::Specialization::Symbol::Invalid);
            const std::size_t separator = field.find("::");
            if (separator == std::string_view::npos) {
                signature.symbol.assign(field);
            } else {
                if (separator == 0 || separator + 2 == field.size() ||
                    field.find("::", separator + 2) != std::string_view::npos)
                    raise(Runtime::Errors::Metadata::Specialization::Symbol::Invalid);
                signature.module.assign(field.substr(0, separator));
                signature.symbol.assign(field.substr(separator + 2));
            }
            line.remove_prefix(tab + 1);
            while (!line.empty()) {
                const std::size_t next_tab = line.find('\t');
                const std::string_view argument = line.substr(0, next_tab);
                const std::size_t colon = argument.find(':');
                if (colon == std::string_view::npos || colon == 0)
                    raise(Runtime::Errors::Metadata::Specialization::Argument::Invalid,
                          signature.path);
                Artifact::Input input;
                input.dtype.assign(argument.substr(0, colon));
                std::string_view dimensions = argument.substr(colon + 1);
                while (!dimensions.empty()) {
                    const std::size_t comma = dimensions.find(',');
                    const std::string token(dimensions.substr(0, comma));
                    std::size_t consumed = 0;
                    const long long dimension = std::stoll(token, &consumed);
                    if (consumed != token.size() || dimension < -1)
                        raise(Runtime::Errors::Metadata::Specialization::Shape::Invalid,
                              signature.path);
                    input.shape.push_back(static_cast<int64_t>(dimension));
                    if (comma == std::string_view::npos)
                        break;
                    dimensions.remove_prefix(comma + 1);
                }
                signature.inputs.push_back(std::move(input));
                if (next_tab == std::string_view::npos)
                    break;
                line.remove_prefix(next_tab + 1);
            }
            const bool duplicated =
                std::any_of(result.begin(), result.end(), [&](const Artifact::Signature &other) {
                    return other.path == signature.path && other.module == signature.module &&
                           other.symbol == signature.symbol;
                });
            if (duplicated)
                raise(Runtime::Errors::Metadata::Specialization::Duplicated, signature.symbol);
            result.push_back(std::move(signature));
        }
        return result;
    }

    std::vector<std::pair<std::string, uint32_t>> decode_deployment(std::span<const uint8_t> bytes,
                                                                    std::size_t device_count) {
        std::vector<std::pair<std::string, uint32_t>> deployment;
        std::string_view text = as_text(bytes);
        for (std::string_view line; next_line(text, line);) {
            if (line.empty())
                raise(Runtime::Errors::Metadata::Deployment::Line::Empty);
            const std::size_t tab = line.find('\t');
            if (tab == std::string_view::npos || tab == 0 || tab + 1 == line.size())
                raise(Runtime::Errors::Metadata::Deployment::Invalid);
            const std::string index(line.substr(0, tab));
            std::size_t consumed = 0;
            const unsigned long parsed = std::stoul(index, &consumed);
            if (consumed != index.size() || parsed >= device_count)
                raise(Runtime::Errors::Metadata::Deployment::Device::Invalid);
            std::string path(line.substr(tab + 1));
            const bool duplicated =
                std::any_of(deployment.begin(), deployment.end(),
                            [&](const auto &entry) { return entry.first == path; });
            if (duplicated)
                raise(Runtime::Errors::Metadata::Deployment::Entrypoint::Duplicated, path);
            deployment.emplace_back(std::move(path), static_cast<uint32_t>(parsed));
        }
        return deployment;
    }

    Artifact::Metadata read_artifact_metadata(std::string_view path) {
        const std::shared_ptr<Package::File> archive = Package::File::open(path);
        Artifact::Metadata metadata;
        metadata.path.assign(path);
        metadata.file_bytes = archive->size();
        metadata.format_version = archive->version();

        for (const Package::Section &section : archive->sections()) {
            metadata.sections.push_back(Artifact::Section{
                section.type, std::string(section.name), section.bytes.size(),
                section.raw_type, section.optional});
            if (section.type == Package::Section::Type::Parameters)
                metadata.parameter_section_bytes += section.bytes.size();
        }

        const auto single = [&](Package::Section::Type type) -> std::span<const uint8_t> {
            const std::vector<Package::Section> found = archive->sections(type);
            if (found.size() > 1)
                raise(Runtime::Errors::Metadata::Section::Duplicated);
            return found.empty() ? std::span<const uint8_t>{} : found.front().bytes;
        };

        const std::span<const uint8_t> devices = single(Package::Section::Type::Devices);
        if (!devices.empty()) {
            metadata.devices = decode_devices(devices);
            std::unordered_set<std::string> seen;
            for (const std::string &spec : metadata.devices) {
                std::size_t begin = 0;
                while (begin <= spec.size()) {
                    const std::size_t separator = spec.find(',', begin);
                    const std::string_view uri(
                        spec.data() + begin,
                        (separator == std::string::npos ? spec.size() : separator) - begin);
                    const std::size_t scheme = uri.find("://");
                    std::string backend(uri.substr(0, scheme));
                    if (backend == "local-task" || backend == "local-sync")
                        backend = "local";
                    if (!backend.empty() && seen.insert(backend).second)
                        metadata.backends.push_back(std::move(backend));
                    if (separator == std::string::npos)
                        break;
                    begin = separator + 1;
                }
            }
        }

        const std::span<const uint8_t> application =
            single(Package::Section::Type::ApplicationEntrypoints);
        if (!application.empty())
            metadata.entrypoints = decode_application_entrypoints(application);

        const std::span<const uint8_t> hooks = single(Package::Section::Type::Hooks);
        if (!hooks.empty())
            metadata.hooks = decode_hooks(hooks);

        const std::span<const uint8_t> signatures = single(Package::Section::Type::Entrypoints);
        if (!signatures.empty())
            metadata.signatures = decode_signatures(signatures);

        const std::span<const uint8_t> deployment = single(Package::Section::Type::Deployment);
        if (!deployment.empty())
            metadata.deployment = decode_deployment(deployment, metadata.devices.size());

        const std::span<const uint8_t> requirements =
            single(Package::Section::Type::Requirements);
        if (!requirements.empty())
            metadata.requirements = decode_requirements(requirements);

        for (const Package::Section &index :
             archive->sections(Package::Section::Type::WeightIndex)) {
            if (index.name != "requirements")
                continue;
            metadata.parameters =
                Weight::decode_weight_index(index.bytes, Weight::Index::Kind::Requirements);
            for (const Weight::Descriptor &parameter : metadata.parameters)
                metadata.parameter_bytes += parameter.byte_length;
        }
        return metadata;
    }
} // namespace Mln
