#pragma once

#include "mln/error.hpp"
#include "mln/weight_errors.hpp"

#include <bit>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Mln::Weight {
    enum class Residency : uint32_t {
        Retained = 0,
        Volatile = 1,
    };

    namespace Index {
        enum class Kind : uint32_t {
            Requirements = 1,
            Descriptors = 2,
        };
    }

    struct Descriptor {
            std::string name;
            std::string quant;
            std::string storage;
            std::vector<int64_t> shape;
            uint64_t byte_length = 0;
            Residency residency = Residency::Retained;
    };
    namespace Encoding {
        constexpr uint8_t weight_magic[8] = {'M', 'L', 'N', 'W', 'G', 'T', 0, 2};

        constexpr std::size_t min_descriptor_bytes = 5 * 4 + 8 + 2;

        inline void append_u32(std::vector<uint8_t> &out, uint32_t value) {
            for (unsigned i = 0; i < 4; ++i)
                out.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }

        inline void append_u64(std::vector<uint8_t> &out, uint64_t value) {
            for (unsigned i = 0; i < 8; ++i)
                out.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }

        class Reader {
            public:
                explicit Reader(std::span<const uint8_t> bytes) : bytes_(bytes) {}

                uint32_t u32() {
                    require(4);
                    uint32_t value = 0;
                    for (unsigned i = 0; i < 4; ++i)
                        value |= static_cast<uint32_t>(bytes_[offset_++]) << (i * 8);
                    return value;
                }

                uint64_t u64() {
                    require(8);
                    uint64_t value = 0;
                    for (unsigned i = 0; i < 8; ++i)
                        value |= static_cast<uint64_t>(bytes_[offset_++]) << (i * 8);
                    return value;
                }

                std::string string() {
                    const uint32_t length = u32();
                    if (length > 4096)
                        raise(Weight::Errors::Index::String::Too::Large);
                    return fixed_string(length);
                }

                std::string fixed_string(uint32_t length) {
                    require(length);
                    std::string value(reinterpret_cast<const char *>(bytes_.data() + offset_),
                                      length);
                    offset_ += length;
                    return value;
                }

                void require(std::size_t length) const {
                    if (length > bytes_.size() - offset_)
                        raise(Weight::Errors::Index::Truncated);
                }

                std::size_t remaining() const { return bytes_.size() - offset_; }

            private:
                std::span<const uint8_t> bytes_;
                std::size_t offset_ = 0;
        };
    }

    inline std::vector<uint8_t>
    encode_weight_index(Weight::Index::Kind kind, const std::vector<Weight::Descriptor> &entries) {
        static_assert(std::endian::native == std::endian::little,
                      "WeightIndex requer uma plataforma little-endian");
        if (entries.size() > std::numeric_limits<uint32_t>::max())
            raise(Weight::Errors::Index::Too::Many::Entries);

        std::vector<uint8_t> out(std::begin(Encoding::weight_magic),
                                 std::end(Encoding::weight_magic));
        Encoding::append_u32(out, static_cast<uint32_t>(kind));
        Encoding::append_u32(out, static_cast<uint32_t>(entries.size()));
        std::unordered_set<std::string> names;
        for (const Weight::Descriptor &entry : entries) {
            if (entry.name.empty() || entry.quant.empty())
                raise(Weight::Errors::Descriptor::Incomplete, entry.name);
            if (!names.insert(entry.name).second)
                raise(Weight::Errors::Duplicated, entry.name);
            if (entry.name.size() > 4096 || entry.quant.size() > 4096 ||
                entry.storage.size() > 4096 || entry.shape.size() > 64)
                raise(Weight::Errors::Descriptor::Too::Large, entry.name);

            Encoding::append_u32(out, static_cast<uint32_t>(entry.name.size()));
            Encoding::append_u32(out, static_cast<uint32_t>(entry.quant.size()));
            Encoding::append_u32(out, static_cast<uint32_t>(entry.storage.size()));
            Encoding::append_u32(out, static_cast<uint32_t>(entry.shape.size()));
            Encoding::append_u64(out, entry.byte_length);
            Encoding::append_u32(out, static_cast<uint32_t>(entry.residency));
            out.insert(out.end(), entry.name.begin(), entry.name.end());
            out.insert(out.end(), entry.quant.begin(), entry.quant.end());
            out.insert(out.end(), entry.storage.begin(), entry.storage.end());
            for (int64_t dimension : entry.shape) {
                if (dimension <= 0)
                    raise(Weight::Errors::Descriptor::Dimension::Invalid, entry.name);
                Encoding::append_u64(out, static_cast<uint64_t>(dimension));
            }
        }
        return out;
    }

    inline std::vector<Weight::Descriptor> decode_weight_index(std::span<const uint8_t> bytes,
                                                               Weight::Index::Kind expected_kind) {
        if (bytes.size() < 16 || std::memcmp(bytes.data(), Encoding::weight_magic, 8) != 0)
            raise(Weight::Errors::Index::Magic::Mismatch);
        Encoding::Reader payload(bytes.subspan(8));
        if (payload.u32() != static_cast<uint32_t>(expected_kind))
            raise(Weight::Errors::Index::Kind::Mismatch);
        const uint32_t count = payload.u32();
        if (count > payload.remaining() / Encoding::min_descriptor_bytes)
            raise(Weight::Errors::Index::Too::Many::Entries);

        std::vector<Weight::Descriptor> entries;
        entries.reserve(count);
        std::unordered_set<std::string> names;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t name_length = payload.u32();
            const uint32_t quant_length = payload.u32();
            const uint32_t storage_length = payload.u32();
            const uint32_t rank = payload.u32();
            const uint64_t byte_length = payload.u64();
            const uint32_t residency = payload.u32();
            if (name_length == 0 || name_length > 4096 || quant_length == 0 ||
                quant_length > 4096 || storage_length > 4096 || rank > 64 ||
                residency > static_cast<uint32_t>(Residency::Volatile))
                raise(Weight::Errors::Descriptor::Invalid);
            payload.require(static_cast<std::size_t>(name_length) + quant_length + storage_length +
                            static_cast<std::size_t>(rank) * 8);

            Weight::Descriptor entry;
            entry.name = payload.fixed_string(name_length);
            entry.quant = payload.fixed_string(quant_length);
            entry.storage = payload.fixed_string(storage_length);
            entry.byte_length = byte_length;
            entry.residency = static_cast<Residency>(residency);
            entry.shape.reserve(rank);
            for (uint32_t j = 0; j < rank; ++j) {
                const uint64_t raw = payload.u64();
                if (raw == 0 || raw > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
                    raise(Weight::Errors::Descriptor::Dimension::Invalid, entry.name);
                entry.shape.push_back(static_cast<int64_t>(raw));
            }
            if (!names.insert(entry.name).second)
                raise(Weight::Errors::Duplicated, entry.name);
            entries.push_back(std::move(entry));
        }
        if (payload.remaining() != 0)
            raise(Weight::Errors::Index::Trailing::Bytes);
        return entries;
    }

    inline std::vector<uint8_t> encode_weight_metadata(const Weight::Descriptor &descriptor) {
        return encode_weight_index(Weight::Index::Kind::Descriptors, {descriptor});
    }

    inline Weight::Descriptor decode_weight_metadata(std::span<const uint8_t> bytes) {
        std::vector<Weight::Descriptor> descriptors =
            decode_weight_index(bytes, Weight::Index::Kind::Descriptors);
        if (descriptors.size() != 1)
            raise(Weight::Errors::Descriptor::Not::Single);
        return std::move(descriptors.front());
    }

    inline void validate_weights(const std::vector<Weight::Descriptor> &requirements,
                                 const std::vector<Weight::Descriptor> &descriptors) {
        std::unordered_map<std::string, const Weight::Descriptor *> actual;
        for (const Weight::Descriptor &descriptor : descriptors)
            actual.emplace(descriptor.name, &descriptor);
        for (const Weight::Descriptor &requirement : requirements) {
            const auto found = actual.find(requirement.name);
            if (found == actual.end())
                raise(Weight::Errors::Missing, requirement.name);
            const Weight::Descriptor &descriptor = *found->second;
            const std::string &storage =
                descriptor.storage.empty() ? descriptor.quant : descriptor.storage;
            if (!descriptor.storage.empty() && descriptor.quant != descriptor.storage)
                raise(Weight::Errors::Index::Tampered, requirement.name, descriptor.quant,
                      descriptor.storage);
            if (storage != requirement.quant)
                raise(Weight::Errors::Quant::Mismatch, requirement.name, requirement.quant,
                      storage);
            if (descriptor.shape != requirement.shape)
                raise(Weight::Errors::Shape::Mismatch, requirement.name);
            if (descriptor.byte_length == 0)
                raise(Weight::Errors::Storage::Empty, requirement.name);
        }
    }

    inline std::vector<uint8_t> read_binary_file(std::string_view path) {
        std::ifstream input(std::string(path), std::ios::binary | std::ios::ate);
        if (!input)
            raise(Weight::Errors::File::Unopenable, path);
        const std::streamsize size = input.tellg();
        if (size < 0)
            raise(Weight::Errors::File::Size::Invalid, path);
        std::vector<uint8_t> bytes(static_cast<std::size_t>(size));
        input.seekg(0);
        if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), size))
            raise(Weight::Errors::File::Read::Failed, path);
        return bytes;
    }

    inline void write_binary_file(std::string_view path, std::span<const uint8_t> bytes) {
        std::ofstream output(std::string(path), std::ios::binary | std::ios::trunc);
        if (!output ||
            (!bytes.empty() && !output.write(reinterpret_cast<const char *>(bytes.data()),
                                             static_cast<std::streamsize>(bytes.size()))))
            raise(Weight::Errors::File::Write::Failed, path);
    }
}
