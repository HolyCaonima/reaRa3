#pragma once

#include <string>
#include <string_view>

namespace ra3 {

inline std::string_view trim(std::string_view text)
{
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!text.empty() && isSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

inline char lowerAscii(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

inline std::string lowerAscii(std::string_view text)
{
    std::string result(text);
    for (char& c : result) {
        c = lowerAscii(c);
    }
    return result;
}

inline bool equalsIgnoreCaseAscii(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lowerAscii(a[i]) != lowerAscii(b[i])) {
            return false;
        }
    }
    return true;
}

// Splits `text` at the first '\n'; returns the line without its terminator and
// advances `text` past it.
inline std::string_view takeLine(std::string_view& text)
{
    const std::size_t newline = text.find('\n');
    const std::string_view line = text.substr(0, newline);
    text.remove_prefix(newline == std::string_view::npos ? text.size() : newline + 1);
    return line;
}

}  // namespace ra3
