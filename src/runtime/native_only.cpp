#include "mln/c/runtime.h"

#include "mln/package.hpp"
#include "mln/runtime/metadata.hpp"
#include "mln/sdk/native/abi.hpp"
#include "native_registry.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct mln_runtime_t {};

struct mln_buffer_t {
    std::vector<uint8_t> bytes;
    std::vector<int64_t> shape;
};

struct NativeEntrypoint {
    std::string name;
    std::string module;
    std::string symbol;
};

struct mln_session_t {
    std::shared_ptr<Mln::Package::File> archive;
    Mln::Native::Registry registry;
    std::vector<NativeEntrypoint> entrypoints;
    std::vector<Mln::Package::Section> assets;
    std::mutex mutex;
};

namespace {
    thread_local std::string diagnostic_message;
    thread_local std::string requirement_platform;
    thread_local mln_diagnostic_t diagnostic{};

    mln_status_t fail(std::string message, mln_status_t status = MLN_STATUS_RUNTIME_ERROR) {
        diagnostic_message = std::move(message);
        diagnostic = {"myelin.native", 1, diagnostic_message.c_str(), "", MLN_DIAGNOSTIC_ERROR};
        return status;
    }

    template <typename Function> mln_status_t guarded(Function &&function) {
        try {
            function();
            diagnostic_message.clear();
            diagnostic = {};
            return MLN_STATUS_OK;
        } catch (const std::bad_alloc &) {
            return fail("out of memory", MLN_STATUS_OUT_OF_MEMORY);
        } catch (const std::exception &error) {
            return fail(error.what());
        } catch (...) {
            return fail("unknown native runtime error");
        }
    }

    std::vector<NativeEntrypoint> decode_native_entrypoints(std::span<const uint8_t> bytes) {
        std::vector<NativeEntrypoint> result;
        std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        while (!text.empty()) {
            const std::size_t newline = text.find('\n');
            std::string_view line = text.substr(0, newline);
            text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
            if (line.empty()) continue;
            const std::size_t first = line.find('\t');
            const std::size_t second = line.find('\t', first + 1);
            const std::size_t third = line.find('\t', second + 1);
            if (first == std::string_view::npos || second == std::string_view::npos ||
                third == std::string_view::npos || line.substr(first + 1, second - first - 1) != "native")
                throw std::runtime_error("native-only runtime requires native entrypoints");
            result.push_back({std::string(line.substr(0, first)),
                              std::string(line.substr(second + 1, third - second - 1)),
                              std::string(line.substr(third + 1))});
        }
        if (result.empty()) throw std::runtime_error("artifact has no native entrypoint");
        return result;
    }

    struct InvocationBuffers {
        std::unordered_map<uint32_t, std::unique_ptr<mln_buffer_t>> owned;
        std::unordered_map<uint32_t, std::span<const uint8_t>> views;
        uint32_t next = 1;

        uint32_t add_view(std::span<const uint8_t> bytes) {
            const uint32_t id = next++;
            views.emplace(id, bytes);
            return id;
        }

        uint32_t allocate(uint32_t size) {
            const uint32_t id = next++;
            auto buffer = std::make_unique<mln_buffer_t>();
            buffer->bytes.resize(size);
            owned.emplace(id, std::move(buffer));
            return id;
        }

        std::span<const uint8_t> read(uint32_t id) const {
            if (const auto found = owned.find(id); found != owned.end()) return found->second->bytes;
            if (const auto found = views.find(id); found != views.end()) return found->second;
            throw std::runtime_error("native provider used an unknown buffer");
        }
    };

    uint32_t buffer_allocate(void *user_data, uint32_t bytes) {
        return static_cast<InvocationBuffers *>(user_data)->allocate(bytes);
    }
    void buffer_release(void *user_data, uint32_t buffer) {
        static_cast<InvocationBuffers *>(user_data)->owned.erase(buffer);
    }
    uint32_t buffer_length(void *user_data, uint32_t buffer) {
        return static_cast<uint32_t>(static_cast<InvocationBuffers *>(user_data)->read(buffer).size());
    }
    void buffer_read(void *user_data, uint32_t buffer, uint32_t offset, void *destination,
                     uint32_t size) {
        const auto bytes = static_cast<InvocationBuffers *>(user_data)->read(buffer);
        if (offset > bytes.size() || size > bytes.size() - offset)
            throw std::runtime_error("native buffer read is out of bounds");
        std::memcpy(destination, bytes.data() + offset, size);
    }
    void buffer_write(void *user_data, uint32_t buffer, uint32_t offset, const void *source,
                      uint32_t size) {
        auto &buffers = *static_cast<InvocationBuffers *>(user_data);
        const auto found = buffers.owned.find(buffer);
        if (found == buffers.owned.end() || offset > found->second->bytes.size() ||
            size > found->second->bytes.size() - offset)
            throw std::runtime_error("native buffer write is out of bounds");
        std::memcpy(found->second->bytes.data() + offset, source, size);
    }
    const void *buffer_data(void *user_data, uint32_t buffer) {
        return static_cast<InvocationBuffers *>(user_data)->read(buffer).data();
    }
}

extern "C" {
uint32_t mln_c_api_version(void) { return MLN_C_API_VERSION; }
const mln_diagnostic_t *mln_last_diagnostic(void) {
    return diagnostic.message ? &diagnostic : nullptr;
}
const char *mln_status_string(mln_status_t status) {
    switch (status) {
    case MLN_STATUS_OK: return "ok";
    case MLN_STATUS_INVALID_ARGUMENT: return "invalid argument";
    case MLN_STATUS_OUT_OF_MEMORY: return "out of memory";
    case MLN_STATUS_CALLBACK_ERROR: return "callback error";
    default: return "runtime error";
    }
}

mln_status_t mln_runtime_create(mln_runtime_t **output) {
    if (!output) return fail("runtime output is null", MLN_STATUS_INVALID_ARGUMENT);
    return guarded([&] { *output = new mln_runtime_t; });
}
mln_status_t mln_runtime_create_with_config(const char *, mln_runtime_t **output) {
    return mln_runtime_create(output);
}
void mln_runtime_destroy(mln_runtime_t *runtime) { delete runtime; }

mln_status_t mln_session_create(mln_runtime_t *runtime, mln_session_t **output) {
    if (!runtime || !output) return fail("invalid session create arguments", MLN_STATUS_INVALID_ARGUMENT);
    return guarded([&] { *output = new mln_session_t; });
}
void mln_session_destroy(mln_session_t *session) { delete session; }

mln_status_t mln_session_load(mln_session_t *session, const char *path) {
    if (!session || !path) return fail("invalid load arguments", MLN_STATUS_INVALID_ARGUMENT);
    return guarded([&] {
        auto archive = Mln::Package::File::open(path);
        const auto requirements = Mln::decode_requirements(
            archive->required(Mln::Package::Section::Type::Requirements, "runtime").bytes);
        if (!requirements.platform.empty() && requirements.platform != Mln::current_platform())
            throw std::runtime_error("artifact platform does not match runtime");
        Mln::Native::Registry registry;
        for (const auto &native : archive->sections(Mln::Package::Section::Type::Native))
            registry.load_memory(native.name, native.bytes);
        auto entrypoints = decode_native_entrypoints(
            archive->required(Mln::Package::Section::Type::ApplicationEntrypoints, "public").bytes);
        session->assets = archive->sections(Mln::Package::Section::Type::Asset);
        session->archive = std::move(archive);
        session->registry = std::move(registry);
        session->entrypoints = std::move(entrypoints);
    });
}
mln_status_t mln_session_load_backend(mln_session_t *session, const char *path, const char *) {
    return mln_session_load(session, path);
}

mln_status_t mln_session_invoke(mln_session_t *session, const char *entrypoint,
                                const uint8_t *input, size_t input_size,
                                mln_output_callback_t callback, void *user_data) {
    if (!session || (!input && input_size) || !callback)
        return fail("invalid invoke arguments", MLN_STATUS_INVALID_ARGUMENT);
    mln_status_t callback_status = MLN_STATUS_OK;
    const mln_status_t status = guarded([&] {
        std::lock_guard lock(session->mutex);
        const auto selected = entrypoint
            ? std::ranges::find_if(session->entrypoints, [&](const auto &item) {
                  return item.name == entrypoint;
              })
            : (session->entrypoints.size() == 1 ? session->entrypoints.begin()
                                                : session->entrypoints.end());
        if (selected == session->entrypoints.end()) throw std::runtime_error("unknown entrypoint");
        const auto *symbol = session->registry.resolve(selected->module, selected->symbol);
        if (!symbol) throw std::runtime_error("native entrypoint symbol is missing");
        InvocationBuffers buffers;
        std::vector<uint64_t> arguments;
        arguments.push_back(buffers.add_view({input, input_size}));
        for (const auto &asset : session->assets) arguments.push_back(buffers.add_view(asset.bytes));
        if (symbol->signature && symbol->signature->argument_count != arguments.size())
            throw std::runtime_error("native entrypoint argument count does not match package assets");
        uint64_t result = 0;
        Mln::Sdk::Native::Context context{
            &buffers,
            {buffer_allocate, buffer_release, buffer_length, buffer_read, buffer_write, buffer_data}};
        Mln::Sdk::Native::Call call{arguments.data(), static_cast<uint32_t>(arguments.size()),
                                    &result, 1};
        symbol->function(&context, &call);
        const auto output = buffers.read(static_cast<uint32_t>(result));
        if (callback(output.data(), output.size(), user_data) != 0)
            callback_status = MLN_STATUS_CALLBACK_ERROR;
    });
    return status == MLN_STATUS_OK ? callback_status : status;
}

mln_status_t mln_buffer_create(size_t size, const int64_t *shape, size_t rank, mln_buffer_t **out) {
    if (!out || (rank && !shape)) return fail("invalid buffer arguments", MLN_STATUS_INVALID_ARGUMENT);
    return guarded([&] {
        auto buffer = std::make_unique<mln_buffer_t>();
        buffer->bytes.resize(size);
        if (rank) buffer->shape.assign(shape, shape + rank);
        *out = buffer.release();
    });
}
mln_status_t mln_buffer_clone(const mln_buffer_t *buffer, mln_buffer_t **out) {
    if (!buffer || !out) return fail("invalid buffer clone arguments", MLN_STATUS_INVALID_ARGUMENT);
    return guarded([&] { *out = new mln_buffer_t(*buffer); });
}
void mln_buffer_destroy(mln_buffer_t *buffer) { delete buffer; }
size_t mln_buffer_size(const mln_buffer_t *buffer) { return buffer ? buffer->bytes.size() : 0; }
size_t mln_buffer_rank(const mln_buffer_t *buffer) { return buffer ? buffer->shape.size() : 0; }
int64_t mln_buffer_dimension(const mln_buffer_t *buffer, size_t index) {
    return buffer && index < buffer->shape.size() ? buffer->shape[index] : 0;
}
mln_status_t mln_buffer_read(const mln_buffer_t *buffer, size_t offset, uint8_t *dst, size_t size) {
    if (!buffer || !dst || offset > buffer->bytes.size() || size > buffer->bytes.size() - offset)
        return fail("invalid buffer read", MLN_STATUS_INVALID_ARGUMENT);
    std::memcpy(dst, buffer->bytes.data() + offset, size); return MLN_STATUS_OK;
}
mln_status_t mln_buffer_write(mln_buffer_t *buffer, size_t offset, const uint8_t *src, size_t size) {
    if (!buffer || !src || offset > buffer->bytes.size() || size > buffer->bytes.size() - offset)
        return fail("invalid buffer write", MLN_STATUS_INVALID_ARGUMENT);
    std::memcpy(buffer->bytes.data() + offset, src, size); return MLN_STATUS_OK;
}

mln_status_t mln_runtime_enumerate_devices(mln_runtime_t *runtime, mln_device_callback_t callback,
                                           void *user_data) {
    if (!runtime || !callback) return fail("invalid device enumeration", MLN_STATUS_INVALID_ARGUMENT);
    const mln_device_t device{"native", "local", "MyeLiN native provider"};
    return callback(&device, user_data) == 0 ? MLN_STATUS_OK : MLN_STATUS_CALLBACK_ERROR;
}
mln_status_t mln_artifact_enumerate_backends(const char *path, mln_string_callback_t callback,
                                              void *user_data) {
    if (!path || !callback) return fail("invalid backend enumeration", MLN_STATUS_INVALID_ARGUMENT);
    return guarded([&] {
        const auto archive = Mln::Package::File::open(path);
        if (!archive->sections(Mln::Package::Section::Type::Native).empty() &&
            callback("native", user_data) != 0)
            throw std::runtime_error("backend callback stopped enumeration");
    });
}
mln_status_t mln_artifact_enumerate_sections(const char *path, mln_section_callback_t callback,
                                              void *user_data) {
    if (!path || !callback) return fail("invalid section enumeration", MLN_STATUS_INVALID_ARGUMENT);
    return guarded([&] {
        const auto archive = Mln::Package::File::open(path);
        for (const auto &section : archive->sections()) {
            const std::string name(section.name);
            const mln_section_t value{section.raw_type, name.c_str(), section.bytes.size(),
                                      static_cast<uint8_t>(section.optional)};
            if (callback(&value, user_data) != 0)
                throw std::runtime_error("section callback stopped enumeration");
        }
    });
}
mln_status_t mln_artifact_enumerate_entrypoints(const char *path,
                                                 mln_entrypoint_callback_t callback,
                                                 void *user_data) {
    if (!path || !callback) return fail("invalid entrypoint enumeration", MLN_STATUS_INVALID_ARGUMENT);
    return guarded([&] {
        const auto archive = Mln::Package::File::open(path);
        for (const auto &entry : decode_native_entrypoints(
                 archive->required(Mln::Package::Section::Type::ApplicationEntrypoints,
                                   "public").bytes)) {
            const mln_entrypoint_t value{entry.name.c_str(), "native", entry.module.c_str(),
                                         entry.symbol.c_str()};
            if (callback(&value, user_data) != 0)
                throw std::runtime_error("entrypoint callback stopped enumeration");
        }
    });
}
mln_status_t mln_artifact_enumerate_hooks(const char *, mln_entrypoint_callback_t, void *) { return MLN_STATUS_OK; }
mln_status_t mln_artifact_enumerate_signatures(const char *, mln_signature_callback_t, void *) { return MLN_STATUS_OK; }
mln_status_t mln_artifact_enumerate_deployment(const char *, mln_deployment_callback_t, void *) { return MLN_STATUS_OK; }
mln_status_t mln_artifact_enumerate_parameters(const char *, mln_parameter_callback_t, void *) { return MLN_STATUS_OK; }
mln_status_t mln_artifact_requirements(const char *path, mln_requirements_t *output) {
    if (!path || !output) return fail("invalid requirements query", MLN_STATUS_INVALID_ARGUMENT);
    return guarded([&] {
        const auto archive = Mln::Package::File::open(path);
        const auto value = Mln::decode_requirements(
            archive->required(Mln::Package::Section::Type::Requirements, "runtime").bytes);
        requirement_platform = value.platform;
        *output = {value.runtime_abi, value.native_abi, value.driver_abi,
                   requirement_platform.c_str()};
    });
}
mln_status_t mln_session_enumerate_devices(mln_session_t *session, mln_string_callback_t callback,
                                           void *user_data) {
    if (!session || !callback) return fail("invalid session device enumeration", MLN_STATUS_INVALID_ARGUMENT);
    return callback("native:local", user_data) == 0 ? MLN_STATUS_OK : MLN_STATUS_CALLBACK_ERROR;
}
mln_status_t mln_session_enumerate_routes(mln_session_t *, mln_route_callback_t, void *) { return MLN_STATUS_OK; }
mln_status_t mln_session_set_diagnostic_callback(mln_session_t *, mln_diagnostic_callback_t, void *) { return MLN_STATUS_OK; }
void mln_session_cancel(mln_session_t *) {}
mln_status_t mln_session_invoke_buffers(mln_session_t *, const char *, const mln_buffer_t *const *, size_t,
                                        mln_buffer_callback_t, void *) { return MLN_STATUS_RUNTIME_ERROR; }
mln_status_t mln_session_state(mln_session_t *, const char *, size_t, mln_buffer_t **) { return MLN_STATUS_RUNTIME_ERROR; }
mln_status_t mln_session_set_state(mln_session_t *, const char *, const mln_buffer_t *) { return MLN_STATUS_RUNTIME_ERROR; }
mln_status_t mln_session_asset(mln_session_t *, const char *, mln_buffer_t **) { return MLN_STATUS_RUNTIME_ERROR; }
}
