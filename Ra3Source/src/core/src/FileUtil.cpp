#include <ra3/core/FileUtil.h>

#include <fstream>
#include <iterator>

namespace ra3 {

Result<std::string> readWholeFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return makeError("cannot open " + pathToUtf8(path));
    }
    std::string content{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        return makeError("error while reading " + pathToUtf8(path));
    }
    return content;
}

std::string pathToUtf8(const std::filesystem::path& path)
{
    return path.u8string();
}

std::filesystem::path utf8ToPath(const std::string& text)
{
    return std::filesystem::u8path(text);
}

}  // namespace ra3
