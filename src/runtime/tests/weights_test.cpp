#include "mln/error.hpp"
#include "mln/sdk/codec.hpp"
#include "mln/weight.hpp"

#include <cmath>
#include <cstring>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
    class Codec final : public Mln::Sdk::Codec {
        public:
            explicit Codec(std::string label) { name = std::move(label); }

            Mln::Sdk::Tensor decode(Mln::Sdk::Operations &, const Mln::Sdk::Weight &,
                                    Mln::DType) override {
                throw std::runtime_error("Codec: graph decode is not used in this test");
            }

            std::vector<uint8_t> encode(std::span<const float> values) const override {
                std::vector<uint8_t> bytes(values.size() * sizeof(float));
                if (!bytes.empty())
                    std::memcpy(bytes.data(), values.data(), bytes.size());
                return bytes;
            }

            std::vector<float> decode(std::span<const uint8_t> bytes) const override {
                std::vector<float> values(bytes.size() / sizeof(float));
                if (!bytes.empty())
                    std::memcpy(values.data(), bytes.data(), bytes.size());
                return values;
            }
    };
} // namespace

int main(int argc, char **argv) {
    using namespace Mln;
    using namespace Mln::Weight;
    if (argc == 3 && std::string_view(argv[1]) == "--make-safetensors") {
        std::string header =
            R"({"vision.weight":{"dtype":"F32","shape":[512],"data_offsets":[0,2048]},"language.q4":{"dtype":"F32","shape":[512],"data_offsets":[2048,4096]},"language.q6":{"dtype":"F32","shape":[512],"data_offsets":[4096,6144]},"convolution.weight":{"dtype":"F32","shape":[32,16,3,3],"data_offsets":[6144,24576]},"convolution.bias":{"dtype":"F32","shape":[32],"data_offsets":[24576,24704]}})";
        while (header.size() % 8 != 0)
            header.push_back(' ');
        std::vector<uint8_t> file;
        const uint64_t header_size = header.size();
        for (unsigned i = 0; i < 8; ++i)
            file.push_back(static_cast<uint8_t>(header_size >> (i * 8)));
        file.insert(file.end(), header.begin(), header.end());
        for (int tensor = 0; tensor < 3; ++tensor) {
            const std::size_t offset = file.size();
            file.resize(offset + 512 * sizeof(float));
            for (std::size_t i = 0; i < 512; ++i) {
                float value = 2.0f;
                if (tensor == 1)
                    value = (std::sin(static_cast<float>(i) * 0.17f) * 3.0f +
                             std::cos(static_cast<float>(i) * 0.031f)) *
                            1.0e-5f;
                else if (tensor == 2)
                    value = std::cos(static_cast<float>(i) * 0.11f) * 2.0f -
                            std::sin(static_cast<float>(i) * 0.047f) * 0.5f;
                std::memcpy(file.data() + offset + i * sizeof(float), &value, sizeof(float));
            }
        }
        const std::size_t convolution_offset = file.size();
        file.resize(convolution_offset + 4608 * sizeof(float));
        for (std::size_t i = 0; i < 4608; ++i) {
            const float value = std::sin(static_cast<float>(i) * 0.13f) * 1.5f +
                                std::cos(static_cast<float>(i) * 0.037f) * 0.25f;
            std::memcpy(file.data() + convolution_offset + i * sizeof(float), &value,
                        sizeof(float));
        }
        const std::size_t bias_offset = file.size();
        file.resize(bias_offset + 32 * sizeof(float));
        for (std::size_t i = 0; i < 32; ++i) {
            const float bias = 0.25f * static_cast<float>(i + 1);
            std::memcpy(file.data() + bias_offset + i * sizeof(float), &bias, sizeof(float));
        }
        write_binary_file(argv[2], file);
        return 0;
    }
    const std::vector<Weight::Descriptor> requirements{{
        .name = "model.weight",
        .quant = "Q4_K",
        .storage = {},
        .shape = {2, 2},
    }};
    const std::vector<Weight::Descriptor> descriptors{{
        .name = "model.weight",
        .quant = "F32",
        .storage = "F32",
        .shape = {2, 2},
        .byte_length = 16,
        .residency = Weight::Residency::Volatile,
    }};

    if (argc == 2) {
        const std::vector<uint8_t> plan =
            encode_weight_index(Weight::Index::Kind::Requirements, requirements);
        write_binary_file(argv[1], plan);
        return 0;
    }
    if (argc == 3) {
        std::vector<Weight::Descriptor> plan =
            decode_weight_index(read_binary_file(argv[1]), Weight::Index::Kind::Requirements);
        for (Weight::Descriptor &entry : plan)
            entry.quant = "Q4_K";
        write_binary_file(argv[2], encode_weight_index(Weight::Index::Kind::Requirements, plan));
        return 0;
    }

    const std::vector<uint8_t> encoded =
        encode_weight_index(Weight::Index::Kind::Descriptors, descriptors);
    const auto decoded = decode_weight_index(encoded, Weight::Index::Kind::Descriptors);
    if (decoded.size() != 1 || decoded.front().storage != "F32" ||
        decoded.front().byte_length != 16 ||
        decoded.front().residency != Weight::Residency::Volatile)
        throw std::runtime_error("WeightIndex round trip failed");

    const Weight::Descriptor metadata =
        decode_weight_metadata(encode_weight_metadata(descriptors.front()));
    if (metadata.name != "model.weight" || metadata.quant != "F32" ||
        metadata.shape != std::vector<int64_t>{2, 2} ||
        metadata.residency != Weight::Residency::Volatile)
        throw std::runtime_error("IRPA metadata round trip failed");

    std::vector<float> values(256);
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = std::sin(static_cast<float>(i) * 0.17f) * 3.0f +
                    std::cos(static_cast<float>(i) * 0.031f);
    auto check_codec = [&](const Mln::Sdk::Codec &codec, std::size_t expected_bytes,
                           float max_mean_error) {
        const std::vector<uint8_t> bytes = codec.encode(values);
        if (bytes.size() != expected_bytes)
            throw std::runtime_error(codec.name + ": incorrect encoded size");
        const std::vector<float> restored = codec.decode(bytes);
        if (restored.size() != values.size())
            throw std::runtime_error(codec.name + ": incorrect decoded count");
        float error = 0.0f;
        for (std::size_t i = 0; i < values.size(); ++i)
            error += std::abs(values[i] - restored[i]);
        error /= static_cast<float>(values.size());
        if (!std::isfinite(error) || error > max_mean_error)
            throw std::runtime_error(
                codec.name + ": excessive mean quantization error: " + std::to_string(error));
    };
    check_codec(Codec{"F32"}, 1024, 0.0f);
    check_codec(Codec{"F16"}, 1024, 0.0f);
    check_codec(Codec{"Q4_K"}, 1024, 0.0f);
    check_codec(Codec{"Q6_K"}, 1024, 0.0f);

    try {
        validate_weights({{
                             .name = "model.weight",
                             .quant = "Q4_K",
                             .storage = "",
                             .shape = {2, 2},
                         }},
                         {{
                             .name = "model.weight",
                             .quant = "Q4_K",
                             .storage = "F32",
                             .shape = {2, 2},
                             .byte_length = 16,
                         }});
        throw std::runtime_error("tampered Codec/storage index was accepted");
    } catch (const Mln::Diagnostic &error) {
        if (std::string(error.what()).find("tampered index") == std::string::npos)
            throw;
    }

    try {
        validate_weights(requirements, decoded);
    } catch (const Mln::Diagnostic &error) {
        const std::string message = error.what();
        if (message.find("Q4_K") != std::string::npos && message.find("F32") != std::string::npos)
            return 0;
        std::println(stderr, "{}", message);
        return 1;
    }
    std::println(stderr, "Quant incompatibility was not detected");
    return 1;
}
