#pragma once

#include "mln/artifact.hpp"
#include "mln/weight.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Mln {
    namespace Format = Mln::Weight;

    std::vector<std::string> decode_devices(std::span<const uint8_t> bytes);
    std::vector<Artifact::Entrypoint>
    decode_application_entrypoints(std::span<const uint8_t> bytes);
    std::vector<Artifact::Hook> decode_hooks(std::span<const uint8_t> bytes);
    std::vector<Artifact::Signature> decode_signatures(std::span<const uint8_t> bytes);
    std::vector<std::pair<std::string, uint32_t>> decode_deployment(std::span<const uint8_t> bytes,
                                                                    std::size_t device_count);
    std::vector<uint8_t> encode_requirements(const Artifact::Requirements &requirements);
    Artifact::Requirements decode_requirements(std::span<const uint8_t> bytes);
    std::string current_platform();

    Artifact::Metadata read_artifact_metadata(std::string_view path);

    std::vector<Weight::Descriptor> read_artifact_parameters(std::string_view path);
} // namespace Mln
