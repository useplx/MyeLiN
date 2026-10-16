#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Mln {
    namespace Package {
        struct Section {
                enum class Type : uint32_t {
                    Devices = 1,
                    Vmfb = 2,
                    Parameters = 3,
                    Wasm = 4,
                    Native = 5,
                    WeightIndex = 6,
                    Deployment = 7,
                    Asset = 8,
                    Entrypoints = 9,
                    ApplicationEntrypoints = 10,
                    Hooks = 11,
                    Driver = 12,
                    Requirements = 13,
                };

                Type type;
                std::string_view name;
                std::span<const uint8_t> bytes;
                std::uint32_t raw_type = 0;
                bool optional = false;
        };

        class File {
            public:
                static std::shared_ptr<File> open(std::string_view path);

                Section required(Section::Type type) const;
                Section required(Section::Type type, std::string_view name) const;
                std::vector<Section> sections(Section::Type type) const;
                std::vector<Section> sections() const;

                void discard(std::span<const uint8_t> bytes) const;
                std::size_t size() const;
                uint32_t version() const;
                ~File();

                File(const File &) = delete;
                File &operator=(const File &) = delete;

            private:
                File() = default;
                struct Impl;
                std::unique_ptr<Impl> impl_;
        };

        class Builder {
            public:
                void add(Section::Type type, std::string name, std::vector<uint8_t> bytes);
                void add(Section::Type type, std::string name, std::span<const uint8_t> bytes);

                void add_optional(std::uint32_t type, std::string name, std::vector<uint8_t> bytes);
                void write(std::string_view path) const;

            private:
                struct Entry {
                        std::uint32_t type;
                        bool optional = false;
                        std::string name;
                        std::vector<uint8_t> bytes;
                        std::span<const uint8_t> view;
                };
                std::vector<Entry> entries_;
        };
    }
}
