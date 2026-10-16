#pragma once

namespace ra3::io {

// ra3_1.0.game 0x009F3650: case-insensitive compare of NUL-terminated archive
// paths, returning -1, 0 or 1. Equality is ASCII case-insensitive, but ordering
// is not stricmp's: a letter is case-adjusted only when the other side is a
// letter of the opposite case, and bytes compare as signed chars. So "z" > "_"
// while "Z" < "_".
int comparePathNames(const char* a, const char* b);

}  // namespace ra3::io
