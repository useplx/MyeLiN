#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace Mln::Sdk::Native {
    inline std::string sanitize_symbol(std::string_view logical_name) {
        std::string symbol;
        symbol.reserve(logical_name.size());
        for (const unsigned char c : logical_name) {
            symbol.push_back(std::isalnum(c) || c == '_' ? static_cast<char>(c) : '_');
        }
        return symbol;
    }
}
