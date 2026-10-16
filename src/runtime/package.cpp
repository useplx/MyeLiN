#include "mln/package.hpp"
#include "mln/error.hpp"
#include "mln/package_errors.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cstring>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Mln {
    namespace {
        constexpr std::array<uint8_t, 8> magic = {'M', 'Y', 'E', 'L', 'I', 'N', 0, 1};
        constexpr uint32_t format_version = 1;
        constexpr uint64_t section_alignment = 4096;
        constexpr uint32_t optional_section = 1U;

        struct Header {
                std::array<uint8_t, 8> magic;
                uint32_t version;
                uint32_t entry_count;
                uint64_t toc_offset;
                uint64_t file_size;
                uint64_t toc_size;
                uint64_t reserved[3];
        };

        struct Record {
                uint32_t type;
                uint32_t flags;
                uint64_t offset;
                uint64_t length;
                uint32_t name_length;
                uint32_t reserved;
                char name[48];
        };

        static_assert(std::endian::native == std::endian::little,
                      "o formato Myelin v1 requer uma plataforma little-endian");
        static_assert(sizeof(Header) == 64);
        static_assert(sizeof(Record) == 80);

        uint64_t align_up(uint64_t value, uint64_t alignment) {
            if (value > std::numeric_limits<uint64_t>::max() - (alignment - 1))
                raise(Package::Errors::Header::Alignment::Overflow);
            return (value + alignment - 1) & ~(alignment - 1);
        }

        bool valid_type(uint32_t value) {
            return value >= static_cast<uint32_t>(Package::Section::Type::Devices) &&
                   value <= static_cast<uint32_t>(Package::Section::Type::Requirements);
        }

        class Mapping {
            public:
#if defined(_WIN32)
                explicit Mapping(std::string_view path) {
                    std::ifstream input(std::string(path), std::ios::binary | std::ios::ate);
                    if (!input)
                        raise(Package::Errors::File::Unopenable, path, "open failed");
                    const std::streampos end = input.tellg();
                    if (end < 0)
                        raise(Package::Errors::File::Stat::Failed, "tellg failed");
                    bytes_.resize(static_cast<std::size_t>(end));
                    if (bytes_.size() < sizeof(Header))
                        raise(Package::Errors::Header::Truncated);
                    input.seekg(0);
                    input.read(reinterpret_cast<char *>(bytes_.data()),
                               static_cast<std::streamsize>(bytes_.size()));
                    if (!input)
                        raise(Package::Errors::File::Map::Failed, "read failed");
                }
#else
                explicit Mapping(std::string_view path) : descriptor_(path) {
                    struct stat info{};
                    if (fstat(descriptor_.get(), &info) != 0 || info.st_size < 0)
                        raise(Package::Errors::File::Stat::Failed, std::strerror(errno));
                    size_ = static_cast<std::size_t>(info.st_size);
                    if (size_ < sizeof(Header))
                        raise(Package::Errors::Header::Truncated);

                    void *mapping =
                        mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, descriptor_.get(), 0);
                    if (mapping == MAP_FAILED)
                        raise(Package::Errors::File::Map::Failed, std::strerror(errno));
                    region_ =
                        Region(static_cast<const std::uint8_t *>(mapping), Region::Deleter{size_});
                }
#endif

                Mapping(const Mapping &) = delete;
                Mapping &operator=(const Mapping &) = delete;

                [[nodiscard]] const std::uint8_t *data() const noexcept {
#if defined(_WIN32)
                    return bytes_.data();
#else
                    return region_.get();
#endif
                }
                [[nodiscard]] std::size_t size() const noexcept {
#if defined(_WIN32)
                    return bytes_.size();
#else
                    return size_;
#endif
                }

                void discard(std::span<const uint8_t> bytes) const {
#if defined(_WIN32)
                    if (bytes.empty()) return;
                    const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
                    const auto mapping_begin = reinterpret_cast<std::uintptr_t>(data());
                    const auto mapping_end = mapping_begin + size();
                    if (begin < mapping_begin || begin > mapping_end ||
                        bytes.size() > mapping_end - begin)
                        raise(Package::Errors::File::Map::Discard::Outside);
#else
                    if (bytes.empty())
                        return;
                    const auto mapping_begin = reinterpret_cast<std::uintptr_t>(data());
                    const auto mapping_end = mapping_begin + size_;
                    const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
                    if (begin < mapping_begin || begin > mapping_end ||
                        bytes.size() > mapping_end - begin)
                        raise(Package::Errors::File::Map::Discard::Outside);

                    const long raw_page_size = sysconf(_SC_PAGESIZE);
                    const std::size_t page_size = raw_page_size > 0
                                                      ? static_cast<std::size_t>(raw_page_size)
                                                      : static_cast<std::size_t>(4096);
                    const auto aligned_begin = begin - begin % page_size;
                    const auto end = begin + bytes.size();
                    const auto aligned_end = align_up(end, page_size);
                    if (madvise(reinterpret_cast<void *>(aligned_begin),
                                aligned_end - aligned_begin, MADV_DONTNEED) != 0)
                        raise(Package::Errors::File::Map::Discard::Failed, std::strerror(errno));
#endif
                }

            private:
#if defined(_WIN32)
                std::vector<std::uint8_t> bytes_;
#else
                class Descriptor {
                    public:
                        explicit Descriptor(std::string_view path) {
                            const std::string path_string(path);
                            value_ = ::open(path_string.c_str(), O_RDONLY | O_CLOEXEC);
                            if (value_ < 0)
                                raise(Package::Errors::File::Unopenable, path,
                                      std::strerror(errno));
                        }

                        Descriptor(const Descriptor &) = delete;
                        Descriptor &operator=(const Descriptor &) = delete;
                        ~Descriptor() { close(value_); }

                        [[nodiscard]] int get() const noexcept { return value_; }

                    private:
                        int value_;
                };

                struct Region {
                        struct Deleter {
                                std::size_t size = 0;

                                void operator()(const std::uint8_t *data) const noexcept {
                                    if (data)
                                        munmap(const_cast<std::uint8_t *>(data), size);
                                }
                        };

                        Region() noexcept : value(nullptr, Deleter{}) {}
                        Region(const std::uint8_t *data, Deleter deleter) noexcept
                            : value(data, deleter) {}
                        [[nodiscard]] const std::uint8_t *get() const noexcept {
                            return value.get();
                        }

                        std::unique_ptr<const std::uint8_t, Deleter> value;
                };

                Descriptor descriptor_;
                Region region_;
                std::size_t size_ = 0;
#endif
        };
    } // namespace

    struct Package::File::Impl {
            explicit Impl(std::string_view path) : mapping(path) {}

            Mapping mapping;
            uint32_t version = 0;
            std::vector<Package::Section> sections;
    };

    std::shared_ptr<Package::File> Package::File::open(std::string_view path) {
        auto package = std::shared_ptr<Package::File>(new Package::File());
        package->impl_ = std::make_unique<Impl>(path);
        Impl &impl = *package->impl_;
        const std::size_t file_size = impl.mapping.size();
        const std::uint8_t *data = impl.mapping.data();

        const Header &header = *reinterpret_cast<const Header *>(data);
        if (header.magic != magic || header.version != format_version)
            raise(Package::Errors::Header::Magic::Mismatch);
        impl.version = header.version;
        if (header.file_size != file_size)
            raise(Package::Errors::Header::Size::Mismatch);
        if (header.toc_offset != sizeof(Header) ||
            std::ranges::any_of(header.reserved, [](uint64_t value) { return value != 0; }))
            raise(Package::Errors::Section::Table::Invalid);
        if (header.entry_count == 0 ||
            header.entry_count > std::numeric_limits<uint32_t>::max() / sizeof(Record))
            raise(Package::Errors::Section::Count::Invalid);
        const uint64_t expected_toc_size =
            static_cast<uint64_t>(header.entry_count) * sizeof(Record);
        if (header.toc_size != expected_toc_size || header.toc_offset % alignof(Record) != 0 ||
            header.toc_offset > file_size || header.toc_size > file_size - header.toc_offset)
            raise(Package::Errors::Section::Table::Invalid);

        const auto *records = reinterpret_cast<const Record *>(data + header.toc_offset);
        const uint64_t section_begin = align_up(header.toc_offset + header.toc_size,
                                                section_alignment);
        std::vector<std::pair<uint64_t, uint64_t>> ranges;
        ranges.reserve(header.entry_count);
        impl.sections.reserve(header.entry_count);
        for (uint32_t i = 0; i < header.entry_count; ++i) {
            const Record &record = records[i];
            const bool known = valid_type(record.type);
            const bool optional = (record.flags & optional_section) != 0;
            if ((!known && !optional) || (record.flags & ~optional_section) != 0 ||
                record.reserved != 0 ||
                record.name_length > sizeof(record.name) ||
                record.length == 0 || record.offset < section_begin ||
                record.offset % section_alignment != 0 || record.offset > file_size ||
                record.length > file_size - record.offset)
                raise(Package::Errors::Section::Invalid, i);
            const uint64_t end = record.offset + record.length;
            for (const auto &range : ranges) {
                if (record.offset < range.second && end > range.first)
                    raise(Package::Errors::Section::Overlapping);
            }
            ranges.emplace_back(record.offset, end);

            std::string_view name(record.name, record.name_length);
            if (name.empty() || name.find('\0') != std::string_view::npos)
                raise(Package::Errors::Section::Invalid, i);
            for (const Package::Section &section : impl.sections) {
                if (section.raw_type == record.type &&
                    section.name == name)
                    raise(Package::Errors::Section::Duplicated, name);
            }
            impl.sections.push_back(Package::Section{
                static_cast<Package::Section::Type>(record.type), name,
                std::span<const uint8_t>(data + record.offset, record.length), record.type,
                optional});
        }
        return package;
    }

    Package::Section Package::File::required(Package::Section::Type type) const {
        const std::vector<Package::Section> found = sections(type);
        if (found.size() != 1)
            raise(Package::Errors::Section::Not::Unique, static_cast<uint32_t>(type));
        return found.front();
    }

    Package::Section Package::File::required(Package::Section::Type type,
                                             std::string_view name) const {
        for (const Package::Section &section : impl_->sections) {
            if (section.type == type && section.name == name)
                return section;
        }
        raise(Package::Errors::Section::Missing, name);
    }

    std::vector<Package::Section> Package::File::sections(Package::Section::Type type) const {
        std::vector<Package::Section> found;
        for (const Package::Section &section : impl_->sections)
            if (section.type == type)
                found.push_back(section);
        return found;
    }

    std::vector<Package::Section> Package::File::sections() const {
        return impl_->sections;
    }

    void Package::File::discard(std::span<const uint8_t> bytes) const {
        impl_->mapping.discard(bytes);
    }

    std::size_t Package::File::size() const { return impl_->mapping.size(); }

    uint32_t Package::File::version() const { return impl_->version; }

    Package::File::~File() = default;

    void Package::Builder::add(Package::Section::Type type, std::string name,
                               std::vector<uint8_t> bytes) {
        if (name.empty() || name.find('\0') != std::string::npos)
            raise(Package::Errors::Section::Invalid, entries_.size());
        if (name.size() > sizeof(Record{}.name))
            raise(Package::Errors::Section::Name::Too::Long);
        if (bytes.empty())
            raise(Package::Errors::Section::Empty, name);
        for (const Entry &entry : entries_) {
            if (entry.type == static_cast<uint32_t>(type) && entry.name == name)
                raise(Package::Errors::Section::Duplicated, name);
        }
        entries_.push_back(Entry{static_cast<uint32_t>(type), false, std::move(name),
                                 std::move(bytes), {}});
    }

    void Package::Builder::add(Package::Section::Type type, std::string name,
                               std::span<const uint8_t> bytes) {
        if (name.empty() || name.find('\0') != std::string::npos)
            raise(Package::Errors::Section::Invalid, entries_.size());
        if (name.size() > sizeof(Record{}.name))
            raise(Package::Errors::Section::Name::Too::Long);
        if (bytes.empty())
            raise(Package::Errors::Section::Empty, name);
        for (const Entry &entry : entries_) {
            if (entry.type == static_cast<uint32_t>(type) && entry.name == name)
                raise(Package::Errors::Section::Duplicated, name);
        }
        entries_.push_back(
            Entry{static_cast<uint32_t>(type), false, std::move(name), {}, bytes});
    }

    void Package::Builder::add_optional(uint32_t type, std::string name,
                                        std::vector<uint8_t> bytes) {
        if (valid_type(type))
            raise(Package::Errors::Section::Invalid, entries_.size());
        if (name.empty() || name.find('\0') != std::string::npos ||
            name.size() > sizeof(Record{}.name) || bytes.empty())
            raise(Package::Errors::Section::Invalid, entries_.size());
        for (const Entry &entry : entries_)
            if (entry.type == type && entry.name == name)
                raise(Package::Errors::Section::Duplicated, name);
        entries_.push_back(Entry{type, true, std::move(name), std::move(bytes), {}});
    }

    void Package::Builder::write(std::string_view path) const {
        if (entries_.empty())
            raise(Package::Errors::Section::None);
        if (entries_.size() > std::numeric_limits<uint32_t>::max())
            raise(Package::Errors::Section::Too::Many);

        Header header{};
        header.magic = magic;
        header.version = format_version;
        header.entry_count = static_cast<uint32_t>(entries_.size());
        header.toc_offset = sizeof(Header);
        header.toc_size = entries_.size() * sizeof(Record);

        std::vector<Record> records(entries_.size());
        uint64_t cursor = align_up(header.toc_offset + header.toc_size, section_alignment);
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const Entry &entry = entries_[i];
            Record &record = records[i];
            record.type = entry.type;
            record.flags = entry.optional ? optional_section : 0;
            record.offset = cursor;
            record.length = entry.bytes.empty() ? entry.view.size() : entry.bytes.size();
            record.name_length = static_cast<uint32_t>(entry.name.size());
            std::memcpy(record.name, entry.name.data(), entry.name.size());
            if (record.length > std::numeric_limits<uint64_t>::max() - cursor)
                raise(Package::Errors::File::Too::Large);
            cursor = align_up(cursor + record.length, section_alignment);
        }
        header.file_size = cursor;

        std::ofstream output(std::string(path), std::ios::binary | std::ios::trunc);
        if (!output)
            raise(Package::Errors::File::Not::Creatable, path);
        output.write(reinterpret_cast<const char *>(&header), sizeof(header));
        output.write(reinterpret_cast<const char *>(records.data()),
                     static_cast<std::streamsize>(records.size() * sizeof(Record)));

        std::array<char, section_alignment> zeros{};
        uint64_t position = header.toc_offset + header.toc_size;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const uint64_t padding = records[i].offset - position;
            output.write(zeros.data(), static_cast<std::streamsize>(padding));
            const Entry &entry = entries_[i];
            const std::span<const uint8_t> bytes =
                entry.bytes.empty() ? entry.view : std::span<const uint8_t>(entry.bytes);
            output.write(reinterpret_cast<const char *>(bytes.data()),
                         static_cast<std::streamsize>(bytes.size()));
            position = records[i].offset + records[i].length;
        }
        const uint64_t tail = header.file_size - position;
        output.write(zeros.data(), static_cast<std::streamsize>(tail));
        if (!output)
            raise(Package::Errors::File::Write::Failed, path);
    }
} // namespace Mln
