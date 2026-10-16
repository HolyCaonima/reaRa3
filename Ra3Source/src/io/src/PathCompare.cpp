#include "ra3/io/PathCompare.h"

namespace ra3::io {
namespace {

bool isUpper(signed char c)
{
    return c >= 'A' && c <= 'Z';
}

bool isLower(signed char c)
{
    return c >= 'a' && c <= 'z';
}

}  // namespace

int comparePathNames(const char* a, const char* b)
{
    signed char left = 0;
    signed char right = 0;
    do {
        left = static_cast<signed char>(*a++);
        right = static_cast<signed char>(*b++);
        if (isUpper(left) && isLower(right)) {
            left = static_cast<signed char>(left + ('a' - 'A'));
        } else if (isLower(left) && isUpper(right)) {
            left = static_cast<signed char>(left - ('a' - 'A'));
        }
    } while (left != 0 && right != 0 && left == right);
    if (left > right) {
        return 1;
    }
    return left < right ? -1 : 0;
}

}  // namespace ra3::io
