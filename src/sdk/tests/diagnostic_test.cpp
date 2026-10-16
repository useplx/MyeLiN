#include <mln/error.hpp>

#include <string_view>

namespace {
    constexpr Mln::Error PackageWarning{"third-party", 7, "fallback for {}"};

    Mln::DiagnosticAction capture(const Mln::Diagnostic &diagnostic, void *user_data) {
        auto &seen = *static_cast<bool *>(user_data);
        seen = diagnostic.severity() == Mln::Severity::Warning &&
               diagnostic.code() == "third-party:E7" && diagnostic.message() == "fallback for op";
        return Mln::DiagnosticAction::Ignore;
    }

    Mln::DiagnosticAction promote(const Mln::Diagnostic &, void *) {
        return Mln::DiagnosticAction::PromoteToError;
    }
}

int main() {
    bool seen = false;
    {
        Mln::ScopedDiagnosticHandler handler(capture, &seen);
        Mln::warn(PackageWarning, "op");
    }
    if (!seen)
        return 1;

    try {
        Mln::ScopedDiagnosticHandler handler(promote);
        Mln::warn_at(PackageWarning, "model.layer", "op");
        return 2;
    } catch (const Mln::Diagnostic &diagnostic) {
        if (diagnostic.severity() != Mln::Severity::Error ||
            diagnostic.code() != "third-party:E7" || diagnostic.location() != "model.layer")
            return 3;
    }

    return 0;
}
