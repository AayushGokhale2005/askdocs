#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace askdocs::agent {

// Compact workspace file index: one path pool + sorted basename table.
class FileIndex {
public:
    void scan(const std::string& workspace_root, bool include_hidden = false);
    void clear() noexcept;

    std::size_t size() const noexcept { return entries_.size(); }
    const std::string& path_pool() const noexcept { return pool_; }

    std::vector<std::string> complete(std::string_view prefix, std::size_t limit = 8) const;
    std::optional<std::string> resolve_mention(std::string_view mention) const;
    std::vector<std::string> extract_mentions(std::string_view message) const;

private:
    struct Entry {
        uint32_t path_off = 0;
        uint16_t name_off = 0;
        uint16_t name_len = 0;
    };

    std::string pool_;
    std::vector<Entry> entries_;
    std::vector<uint32_t> sorted_;

    std::string_view entry_name(const Entry& e) const noexcept;
    std::string_view entry_path(const Entry& e) const noexcept;
    static int compare_name(const FileIndex* idx, uint32_t a, uint32_t b) noexcept;
};

}  // namespace askdocs::agent
