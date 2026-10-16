#pragma once

#include <ra3/asset/AssetId.h>
#include <ra3/asset/AssetStream.h>
#include <ra3/core/Result.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ra3::asset {

class RelocationTable {
public:
    static Result<RelocationTable> parse(
        ByteView chunk,
        ByteView instance,
        std::string_view memberName,
        std::uint64_t memberOffset);

    const std::vector<std::uint32_t>& sites() const { return m_sites; }
    bool contains(std::uint32_t site) const;

private:
    std::vector<std::uint32_t> m_sites;
};

struct Import {
    std::uint32_t site = 0;
    std::uint32_t referenceIndex = 0;
    AssetId target;
};

class ImportTable {
public:
    static Result<ImportTable> parse(
        ByteView chunk,
        ByteView instance,
        const std::vector<AssetId>& references,
        std::string_view memberName,
        std::uint64_t memberOffset);

    const std::vector<Import>& imports() const { return m_imports; }
    const Import* find(std::uint32_t site) const;

private:
    std::vector<Import> m_imports;
};

class AssetInstance {
public:
    static Result<AssetInstance> parse(
        ByteView instance,
        ByteView relocations,
        ByteView imports,
        const std::vector<AssetId>& references,
        std::string diagnosticName,
        std::uint64_t relocationMemberOffset = 0,
        std::uint64_t importsMemberOffset = 0);

    ByteView bytes() const { return m_instance; }
    const RelocationTable& relocations() const { return m_relocations; }
    const ImportTable& imports() const { return m_imports; }

    Result<std::uint32_t> readU32(std::uint32_t offset) const;
    std::optional<std::uint32_t> relocationTarget(std::uint32_t site) const;
    const Import* importAt(std::uint32_t site) const;
    Result<std::string_view> readString(std::uint32_t descriptorOffset) const;

private:
    ByteView m_instance;
    std::string m_diagnosticName;
    RelocationTable m_relocations;
    ImportTable m_imports;
};

}  // namespace ra3::asset
