#pragma once

#include <cstdint>
#include <cstdio>
#include <format>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Mln {
    enum class Severity : std::uint8_t { Warning, Error };

    enum class DiagnosticAction : std::uint8_t {
        Continue,
        Ignore,
        PromoteToError,
    };

    class Error {
        public:
            constexpr Error(std::uint16_t code, std::string_view pattern) noexcept
                : code_(code), pattern_(pattern) {}

            constexpr Error(std::string_view domain, std::uint16_t code,
                            std::string_view pattern) noexcept
                : domain_(domain), code_(code), pattern_(pattern) {}

            [[nodiscard]] constexpr std::string_view domain() const noexcept { return domain_; }
            [[nodiscard]] constexpr std::uint16_t code() const noexcept { return code_; }
            [[nodiscard]] constexpr std::string_view pattern() const noexcept { return pattern_; }

            friend constexpr bool operator==(const Error &left, const Error &right) noexcept {
                return left.domain_ == right.domain_ && left.code_ == right.code_;
            }

        private:
            std::string_view domain_;
            std::uint16_t code_;
            std::string_view pattern_;
    };

    namespace Range {
        inline constexpr std::uint16_t Sdk = 1000;
        inline constexpr std::uint16_t Buffer = 1100;
        inline constexpr std::uint16_t Weights = 1200;
        inline constexpr std::uint16_t Compiler = 2000;
        inline constexpr std::uint16_t Runtime = 3000;
        inline constexpr std::uint16_t Package = 3600;
        inline constexpr std::uint16_t Native = 3800;
    }

    class Diagnostic final : public std::runtime_error {
        public:
            Diagnostic(const Error &error, std::string message, std::string location = {},
                       Severity severity = Severity::Error)
                : std::runtime_error(message), error_(error), message_(std::move(message)),
                  location_(std::move(location)), severity_(severity) {}

            [[nodiscard]] const Error &error() const noexcept { return error_; }
            [[nodiscard]] std::string code() const {
                const std::string local = std::format("E{}", error_.code());
                return error_.domain().empty() ? local
                                               : std::format("{}:{}", error_.domain(), local);
            }
            [[nodiscard]] std::string_view message() const noexcept { return message_; }
            [[nodiscard]] std::string_view location() const noexcept { return location_; }
            [[nodiscard]] Severity severity() const noexcept { return severity_; }

        private:
            Error error_;
            std::string message_;
            std::string location_;
            Severity severity_;
    };

    using DiagnosticHandler = DiagnosticAction (*)(const Diagnostic &, void *);

    namespace DiagnosticDispatch {
        struct Context {
                DiagnosticHandler handler = nullptr;
                void *user_data = nullptr;
        };

        inline thread_local Context context;
    }

    class ScopedDiagnosticHandler {
        public:
            ScopedDiagnosticHandler(DiagnosticHandler handler, void *user_data = nullptr) noexcept
                : previous_(DiagnosticDispatch::context) {
                DiagnosticDispatch::context = {handler, user_data};
            }

            ~ScopedDiagnosticHandler() { DiagnosticDispatch::context = previous_; }

            ScopedDiagnosticHandler(const ScopedDiagnosticHandler &) = delete;
            ScopedDiagnosticHandler &operator=(const ScopedDiagnosticHandler &) = delete;

        private:
            DiagnosticDispatch::Context previous_;
    };

    inline void report(const Diagnostic &diagnostic, std::FILE *stream = stderr);

    namespace DiagnosticFormatting {
        inline std::string text(std::string value) { return value; }
        inline std::string text(const char *value) {
            return value ? std::string(value) : std::string{};
        }
        inline std::string text(std::string_view value) { return std::string(value); }
        template <typename Number, typename = std::enable_if_t<std::is_arithmetic_v<Number>>>
        std::string text(Number value) {
            return std::to_string(value);
        }

        inline std::string vcompose(const Error &error, std::format_args arguments) {
#if defined(__wasm__)
            (void)arguments;
            return std::string(error.pattern());
#else
            try {
                return std::vformat(error.pattern(), arguments);
            } catch (const std::format_error &) {
                return std::string(error.pattern());
            }
#endif
        }

        template <typename... Arguments>
        std::string compose(const Error &error, const Arguments &...arguments) {
            return std::apply(
                [&error](const auto &...values) {
                    return vcompose(error, std::make_format_args(values...));
                },
                std::tuple{text(arguments)...});
        }
    }

    template <typename... Arguments>
    [[noreturn]] void raise(const Error &error, const Arguments &...arguments) {
#if defined(__wasm__)
        (void)error;
        ((void)arguments, ...);
        __builtin_trap();
#else
        throw Diagnostic(error, DiagnosticFormatting::compose(error, arguments...));
#endif
    }

    template <typename... Arguments>
    void warn(const Error &warning, const Arguments &...arguments) {
#if defined(__wasm__)
        (void)warning;
        ((void)arguments, ...);
#else
        Diagnostic diagnostic(warning, DiagnosticFormatting::compose(warning, arguments...), {},
                              Severity::Warning);
        DiagnosticAction action = DiagnosticAction::Continue;
        if (DiagnosticDispatch::context.handler)
            action = DiagnosticDispatch::context.handler(diagnostic,
                                                         DiagnosticDispatch::context.user_data);
        if (action == DiagnosticAction::PromoteToError)
            throw Diagnostic(warning, std::string(diagnostic.message()), {}, Severity::Error);
        if (action == DiagnosticAction::Continue)
            report(diagnostic);
#endif
    }

    template <typename... Arguments>
    void warn_at(const Error &warning, std::string location, const Arguments &...arguments) {
#if defined(__wasm__)
        (void)warning;
        (void)location;
        ((void)arguments, ...);
#else
        Diagnostic diagnostic(warning, DiagnosticFormatting::compose(warning, arguments...),
                              std::move(location), Severity::Warning);
        DiagnosticAction action = DiagnosticAction::Continue;
        if (DiagnosticDispatch::context.handler)
            action = DiagnosticDispatch::context.handler(diagnostic,
                                                         DiagnosticDispatch::context.user_data);
        if (action == DiagnosticAction::PromoteToError)
            throw Diagnostic(warning, std::string(diagnostic.message()),
                             std::string(diagnostic.location()), Severity::Error);
        if (action == DiagnosticAction::Continue)
            report(diagnostic);
#endif
    }

    template <typename... Arguments>
    [[noreturn]] void raise_at(const Error &error, std::string location,
                               const Arguments &...arguments) {
#if defined(__wasm__)
        (void)error;
        (void)location;
        ((void)arguments, ...);
        __builtin_trap();
#else
        throw Diagnostic(error, DiagnosticFormatting::compose(error, arguments...),
                         std::move(location));
#endif
    }

    inline void report(const Diagnostic &diagnostic, std::FILE *stream) {
        const std::string_view kind =
            diagnostic.severity() == Severity::Error ? "error" : "warning";
        if (diagnostic.location().empty())
            std::println(stream, "{}[{}]: {}", kind, diagnostic.code(), diagnostic.message());
        else
            std::println(stream, "{}: {}[{}]: {}", diagnostic.location(), kind, diagnostic.code(),
                         diagnostic.message());
    }
}
