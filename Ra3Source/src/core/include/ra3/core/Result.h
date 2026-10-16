#pragma once

#include <string>
#include <utility>
#include <variant>

namespace ra3 {

struct Error {
    std::string message;
};

inline Error makeError(std::string message)
{
    return Error{std::move(message)};
}

// Either a value or an Error describing why it could not be produced.
template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) : m_storage(std::in_place_index<0>, std::move(value)) {}
    Result(Error error) : m_storage(std::in_place_index<1>, std::move(error)) {}

    bool ok() const { return m_storage.index() == 0; }
    explicit operator bool() const { return ok(); }

    T& value() & { return std::get<0>(m_storage); }
    const T& value() const& { return std::get<0>(m_storage); }
    T&& value() && { return std::get<0>(std::move(m_storage)); }

    const Error& error() const { return std::get<1>(m_storage); }

private:
    std::variant<T, Error> m_storage;
};

}  // namespace ra3
