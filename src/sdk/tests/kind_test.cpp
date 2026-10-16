#include "mln/sdk/kind.hpp"

#include <set>
#include <stdexcept>
#include <string>

int main() {
    using Mln::Sdk::Functional::Kind;
    std::set<std::string> names;
    const int count = static_cast<int>(Kind::Count);
    for (int value = static_cast<int>(Kind::Input); value < count; ++value) {
        const std::string name(Mln::Sdk::Functional::name(static_cast<Kind>(value)));
        if (name.empty() || name == "unknown")
            throw std::runtime_error("operation has no canonical name");
        if (!names.insert(name).second)
            throw std::runtime_error("operation canonical name is duplicated");
    }
    if (Mln::Sdk::Functional::name(Kind::Count) != "unknown")
        throw std::runtime_error("unknown operation name is not stable");
}
