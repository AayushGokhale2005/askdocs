#pragma once

#include <list>
#include <optional>
#include <string>
#include <vector>

namespace askdocs::agent {

struct FileSlice {
    std::string path;
    int start_line = 0;
    int end_line = 0;
    std::vector<std::string> lines;
};

// LRU buffer — never holds the whole codebase, only touched files.
class ContextBuffer {
public:
    explicit ContextBuffer(std::size_t max_files = 4, std::size_t max_lines_per_file = 80);

    std::optional<FileSlice> get(const std::string& path, int focus_line = 0);
    void invalidate(const std::string& path) noexcept;

private:
    struct Entry {
        std::string path;
        FileSlice slice;
    };

    std::size_t max_files_;
    std::size_t max_lines_;
    std::list<Entry> lru_;

    std::list<Entry>::iterator find_entry(const std::string& path) noexcept;
    FileSlice load_slice(const std::string& path, int focus_line) const;
};

}  // namespace askdocs::agent
