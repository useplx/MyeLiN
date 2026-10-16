#pragma once

#include "mln/buffer.hpp"
#include "mln/sdk/native/serialization.hpp"

#include <concepts>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace Mln::Sdk::Native {

    template <typename T> using clean_t = std::remove_cv_t<std::remove_reference_t<T>>;

    template <typename T>
    inline constexpr bool is_scalar_v =
        std::is_same_v<clean_t<T>, int32_t> || std::is_same_v<clean_t<T>, int64_t> ||
        std::is_same_v<clean_t<T>, uint32_t> || std::is_same_v<clean_t<T>, uint64_t> ||
        std::is_same_v<clean_t<T>, float> || std::is_same_v<clean_t<T>, double>;

    template <typename T> struct is_span_const : std::false_type {};
    template <typename T, std::size_t Extent>
    struct is_span_const<std::span<const T, Extent>> : std::true_type {};
    template <typename T> inline constexpr bool is_span_const_v = is_span_const<clean_t<T>>::value;

    template <typename T> struct is_copyable_span : std::false_type {};
    template <typename T, std::size_t Extent>
    struct is_copyable_span<std::span<const T, Extent>>
        : std::bool_constant<std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>> {};
    template <typename T>
    inline constexpr bool is_copyable_span_v = is_copyable_span<clean_t<T>>::value;

    template <typename T> struct is_vector : std::false_type {};
    template <typename T> struct is_vector<std::vector<T>> : std::true_type {};
    template <typename T> inline constexpr bool is_vector_v = is_vector<clean_t<T>>::value;

    template <typename T> struct is_copyable_vector : std::false_type {};
    template <typename T>
    struct is_copyable_vector<std::vector<T>>
        : std::bool_constant<std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>> {};
    template <typename T>
    inline constexpr bool is_copyable_vector_v = is_copyable_vector<clean_t<T>>::value;

    template <typename T>
    inline constexpr bool is_buffer_v = std::is_same_v<::Mln::Buffer, clean_t<T>>;

    template <typename T> inline constexpr bool is_session_v = false;

    template <typename T> inline constexpr bool is_pointer_v = is_buffer_v<T> || is_session_v<T>;

    template <typename T>
    inline constexpr bool has_contract_v = requires(
        Return<void>::Writer &writer, Argument<void>::Reader &reader, const clean_t<T> &value) {
        Return<clean_t<T>>::write(writer, value);
        { Argument<clean_t<T>>::read(reader) } -> std::same_as<clean_t<T>>;
    };

    template <typename T>
    inline constexpr bool is_structured_value_v =
        has_contract_v<T> && !is_scalar_v<T> && !is_pointer_v<T>;

    template <typename T>
    inline constexpr bool is_valid_arg_v =
        (!std::is_reference_v<T> ||
         (is_buffer_v<T> && std::is_const_v<std::remove_reference_t<T>>)) &&
        (is_scalar_v<T> || is_copyable_span_v<T> || is_pointer_v<T> || is_structured_value_v<T>);

    template <typename T>
    inline constexpr bool is_valid_result_v =
        !std::is_reference_v<T> &&
        (std::is_void_v<T> || is_scalar_v<T> || is_pointer_v<T> || is_structured_value_v<T>);

    template <typename T, typename = void> struct physical_type;

    template <typename T>
    struct physical_type<
        T, std::enable_if_t<std::is_same_v<clean_t<T>, int32_t> ||
                            std::is_same_v<clean_t<T>, uint32_t> || is_span_const_v<T> ||
                            is_pointer_v<T> || is_structured_value_v<T>>> {
            using type = uint32_t;
    };

    template <typename T>
    struct physical_type<T, std::enable_if_t<std::is_same_v<clean_t<T>, int64_t> ||
                                             std::is_same_v<clean_t<T>, uint64_t>>> {
            using type = uint64_t;
    };

    template <> struct physical_type<float> {
            using type = float;
    };

    template <> struct physical_type<double> {
            using type = double;
    };

    template <> struct physical_type<void> {
            using type = void;
    };

    template <typename T> using physical_type_t = typename physical_type<clean_t<T>>::type;

    template <typename Signature> struct physical_signature;

    template <typename Ret, typename... Args> struct physical_signature<Ret(Args...)> {
            using type = physical_type_t<Ret>(physical_type_t<Args>...);
    };

    template <typename Signature>
    using physical_signature_t = typename physical_signature<Signature>::type;

    template <typename Signature> struct is_valid_signature : std::false_type {};

    template <typename Ret, typename... Args>
    struct is_valid_signature<Ret(Args...)>
        : std::bool_constant<is_valid_result_v<Ret> && (is_valid_arg_v<Args> && ...)> {};

    template <typename Signature>
    inline constexpr bool is_valid_signature_v = is_valid_signature<Signature>::value;

    template <typename Left, typename Right,
              bool Valid = is_valid_signature_v<Left> && is_valid_signature_v<Right>>
    struct physical_signatures_match : std::false_type {};

    template <typename Left, typename Right>
    struct physical_signatures_match<Left, Right, true>
        : std::is_same<physical_signature_t<Left>, physical_signature_t<Right>> {};

    template <typename Left, typename Right>
    inline constexpr bool physical_signatures_match_v =
        physical_signatures_match<Left, Right>::value;

}
