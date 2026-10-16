#pragma once

namespace Mln::Sdk::Type {
    template <typename T> inline unsigned char type_token_storage;

    template <typename T> const void *type_token() noexcept { return &type_token_storage<T>; }
}
