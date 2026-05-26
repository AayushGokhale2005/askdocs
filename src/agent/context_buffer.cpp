#include "askdocs/agent/context_buffer.hpp"
#include "askdocs/file_io.hpp"

#include <algorithm>

namespace askdocs::agent {

ContextBuffer::ContextBuffer(std::size_t max_files, std::size_t max_lines_per_file)
    : max_files_(max_files), max_lines_(max_lines_per_file) {}

std::list<ContextBuffer::Entry>::iterator ContextBuffer::find_entry(const std::string& path) noexcept {
    return std::find_if(lru_.begin(), lru_.end(),
                        [&](const Entry& e) { return e.path == path; });
}

FileSlice ContextBuffer::load_slice(const std::string& path, int focus_line) const {
    FileSlice slice;
    slice.path = path;

    const LoadedFile file = load_file(path);
    if (!file.ok || file.lines.empty()) {
        return slice;
    }

    const int total = static_cast<int>(file.lines.size());
    const int half = static_cast<int>(max_lines_ / 2);
    slice.start_line = std::max(0, focus_line - half);
    slice.end_line = std::min(total, slice.start_line + static_cast<int>(max_lines_));
    if (slice.end_line - slice.start_line < static_cast<int>(max_lines_) && slice.start_line > 0) {
        slice.start_line = std::max(0, slice.end_line - static_cast<int>(max_lines_));
    }

    slice.lines.assign(file.lines.begin() + slice.start_line,
                       file.lines.begin() + slice.end_line);
    return slice;
}

std::optional<FileSlice> ContextBuffer::get(const std::string& path, int focus_line) {
    if (auto it = find_entry(path); it != lru_.end()) {
        Entry entry = *it;
        lru_.erase(it);
        lru_.push_front(std::move(entry));
        return lru_.front().slice;
    }

    FileSlice slice = load_slice(path, focus_line);
    if (slice.lines.empty()) {
        return std::nullopt;
    }

    while (lru_.size() >= max_files_) {
        lru_.pop_back();
    }

    lru_.push_front(Entry{path, slice});
    return slice;
}

void ContextBuffer::invalidate(const std::string& path) noexcept {
    lru_.remove_if([&](const Entry& e) { return e.path == path; });
}

}  // namespace askdocs::agent
