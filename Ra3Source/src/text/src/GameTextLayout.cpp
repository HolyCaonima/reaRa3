#include <ra3/text/GameTextLayout.h>

namespace ra3::text {
namespace {

unsigned char foldAscii(unsigned char value) noexcept
{
    return value >= 'A' && value <= 'Z'
        ? static_cast<unsigned char>(value + ('a' - 'A'))
        : value;
}

int compareLabels(const char* left, const char* right) noexcept
{
    while (*left != '\0' && *right != '\0') {
        const unsigned char a = foldAscii(static_cast<unsigned char>(*left));
        const unsigned char b = foldAscii(static_cast<unsigned char>(*right));
        if (a != b) {
            return a < b ? -1 : 1;
        }
        ++left;
        ++right;
    }
    const unsigned char a = foldAscii(static_cast<unsigned char>(*left));
    const unsigned char b = foldAscii(static_cast<unsigned char>(*right));
    return a == b ? 0 : (a < b ? -1 : 1);
}

}  // namespace

const GameUnicodeString* findGameText(
    const GameStringTable* table, const char* label) noexcept
{
    if (table == nullptr || label == nullptr || table->count == 0 || table->lookup == nullptr) {
        return nullptr;
    }

    std::uint32_t first = 0;
    std::uint32_t count = table->count;
    while (count != 0) {
        const std::uint32_t step = count / 2;
        const std::uint32_t middle = first + step;
        const GameTextLookupEntry& entry = table->lookup[middle];
        const char* candidate =
            entry.label == nullptr || entry.label->data == nullptr ? "" : entry.label->data;
        if (compareLabels(candidate, label) < 0) {
            first = middle + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }

    if (first == table->count) {
        return nullptr;
    }
    const GameTextLookupEntry& found = table->lookup[first];
    const char* candidate =
        found.label == nullptr || found.label->data == nullptr ? "" : found.label->data;
    if (compareLabels(label, candidate) != 0 || found.info == nullptr) {
        return nullptr;
    }
    return &found.info->text;
}

const GameUnicodeString* findGameText(
    const GameTextManager* manager, const char* label) noexcept
{
    if (manager == nullptr) {
        return nullptr;
    }
    if (const GameUnicodeString* result = findGameText(manager->overrideTable, label)) {
        return result;
    }
    if (const GameUnicodeString* result = findGameText(manager->primary, label)) {
        return result;
    }
    return findGameText(manager->map, label);
}

}  // namespace ra3::text
