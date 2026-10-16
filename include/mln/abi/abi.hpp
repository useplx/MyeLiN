#pragma once

#include <cstdint>

namespace Mln::Sdk::Abi {
#if defined(__wasm__)
    using ptr_t = uint32_t;
    using const_ptr_t = uint32_t;

    namespace Buffer {
        __attribute__((import_module("myelin"), import_name("buffer_byte_at"))) uint8_t
        byte_at(uint32_t buffer, uint32_t index);

        __attribute__((import_module("myelin"), import_name("buffer_length"))) uint32_t
        length(uint32_t buffer);

        __attribute__((import_module("myelin"), import_name("buffer_write_byte"))) void
        write_byte(uint32_t buffer, uint32_t index, uint8_t value);

        __attribute__((import_module("myelin"), import_name("buffer_alloc"))) uint32_t
        allocate(uint32_t bytes);

        __attribute__((import_module("myelin"), import_name("buffer_free"))) void
        free(uint32_t buffer);

        __attribute__((import_module("myelin"), import_name("buffer_read"))) void
        read(uint32_t buffer, uint32_t offset, ptr_t dst, uint32_t len);

        __attribute__((import_module("myelin"), import_name("buffer_write"))) void
        write(uint32_t buffer, uint32_t offset, const_ptr_t src, uint32_t len);
    }

    namespace Session {
        __attribute__((import_module("myelin"), import_name("session_run"))) uint32_t
        invoke(uint32_t session, const char *path, uint32_t input);

        __attribute__((import_module("myelin"), import_name("session_run_many"))) uint32_t
        invoke(uint32_t session, const char *path, const_ptr_t inputs, uint32_t input_count,
               ptr_t outputs, uint32_t output_capacity);

        __attribute__((import_module("myelin"), import_name("session_state"))) uint32_t
        state(uint32_t session, const char *name, uint32_t bytes);

        __attribute__((import_module("myelin"), import_name("session_set_state"))) uint32_t
        set_state(uint32_t session, const char *name, uint32_t buffer);

        __attribute__((import_module("myelin"), import_name("session_asset"))) uint32_t
        asset(uint32_t session, const char *name);

        __attribute__((import_module("myelin"), import_name("session_emit"))) void
        emit(uint32_t session, uint32_t buffer, uint32_t offset);
    }
#else
    using ptr_t = void *;
    using const_ptr_t = const void *;

    namespace Buffer {
        uint8_t byte_at(uint32_t buffer, uint32_t index);
        uint32_t length(uint32_t buffer);
        void write_byte(uint32_t buffer, uint32_t index, uint8_t value);
        uint32_t allocate(uint32_t bytes);
        void free(uint32_t buffer);
        void read(uint32_t buffer, uint32_t offset, ptr_t dst, uint32_t len);
        void write(uint32_t buffer, uint32_t offset, const_ptr_t src, uint32_t len);
    }

    namespace Session {
        uint32_t invoke(uint32_t session, const char *path, uint32_t input);
        uint32_t invoke(uint32_t session, const char *path, const_ptr_t inputs,
                        uint32_t input_count, ptr_t outputs, uint32_t output_capacity);
        uint32_t state(uint32_t session, const char *name, uint32_t bytes);
        uint32_t set_state(uint32_t session, const char *name, uint32_t buffer);
        uint32_t asset(uint32_t session, const char *name);
        void emit(uint32_t session, uint32_t buffer, uint32_t offset);
    }
#endif
}
