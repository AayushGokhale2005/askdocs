#include "askdocs/agent/file_index.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <regex>

namespace fs = std::filesystem;

namespace askdocs::agent {

namespace {

bool is_source_file(const fs::path& p) {
    static const char* k_ext[] = {".cpp", ".cc",  ".cxx", ".c",   ".h",   ".hpp", ".py",
                                  ".js",  ".ts",  ".tsx", ".jsx", ".rs",  ".go",  ".md",
                                  ".json", ".yaml", ".yml", ".toml", ".txt", ".cmake"};
    const std::string ext = p.extension().string();
    for (const char* e : k_ext) {
        if (ext == e) {
            return true;
        }
    }
    return p.filename() == "CMakeLists.txt" || p.filename() == "Makefile";
}

std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

}  // namespace

void FileIndex::clear() noexcept {
    pool_.clear();
    entries_.clear();
    sorted_.clear();
}

std::string_view FileIndex::entry_name(const Entry& e) const noexcept {
    return std::string_view(pool_.data() + e.path_off + e.name_off, e.name_len);
}

std::string_view FileIndex::entry_path(const Entry& e) const noexcept {
    const char* start = pool_.data() + e.path_off;
    const char* end = start;
    while (end < pool_.data() + pool_.size() && *end != '\0') {
        ++end;
    }
    return std::string_view(start, static_cast<std::size_t>(end - start));
}

int FileIndex::compare_name(const FileIndex* idx, uint32_t a, uint32_t b) noexcept {
    const std::string_view na = idx->entry_name(idx->entries_[a]);
    const std::string_view nb = idx->entry_name(idx->entries_[b]);
    return na.compare(nb);
}

void FileIndex::scan(const std::string& workspace_root, bool include_hidden) {
    clear();
    std::error_code ec;
    if (!fs::exists(workspace_root, ec)) {
        return;
    }

    std::function<void(const fs::path&, int)> walk = [&](const fs::path& dir, int depth) {
        if (depth > 8 || entries_.size() > 8000) {
            return;
        }
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) {
                break;
            }
            const std::string name = entry.path().filename().string();
            if (!include_hidden && !name.empty() && name[0] == '.') {
                continue;
            }
            if (entry.is_directory(ec)) {
                if (name == "build" || name == "node_modules" || name == ".git") {
                    continue;
                }
                walk(entry.path(), depth + 1);
            } else if (entry.is_regular_file(ec) && is_source_file(entry.path())) {
                const std::string path = fs::weakly_canonical(entry.path(), ec).string();
                const std::string fname = entry.path().filename().string();

                Entry e;
                e.path_off = static_cast<uint32_t>(pool_.size());
                pool_.append(path);
                pool_.push_back('\0');
                e.name_off = static_cast<uint16_t>(path.size() - fname.size());
                e.name_len = static_cast<uint16_t>(fname.size());

                entries_.push_back(e);
            }
        }
    };

    walk(workspace_root, 0);

    sorted_.resize(entries_.size());
    for (uint32_t i = 0; i < entries_.size(); ++i) {
        sorted_[i] = i;
    }
    std::sort(sorted_.begin(), sorted_.end(),
              [this](uint32_t a, uint32_t b) { return compare_name(this, a, b) < 0; });
}

std::vector<std::string> FileIndex::complete(std::string_view prefix, std::size_t limit) const {
    std::vector<std::string> out;
    if (sorted_.empty()) {
        return out;
    }

    const std::string needle = to_lower(std::string(prefix));

    for (uint32_t idx : sorted_) {
        const std::string name = to_lower(std::string(entry_name(entries_[idx])));
        if (!needle.empty() && name.rfind(needle, 0) != 0) {
            if (!out.empty() || name > needle) {
                if (!out.empty()) {
                    break;
                }
            }
            continue;
        }
        if (needle.empty() || name.rfind(needle, 0) == 0) {
            out.emplace_back(entry_path(entries_[idx]));
            if (out.size() >= limit) {
                break;
            }
        }
    }
    return out;
}

std::optional<std::string> FileIndex::resolve_mention(std::string_view mention) const {
    if (mention.empty()) {
        return std::nullopt;
    }

    const std::string needle = to_lower(std::string(mention));

    std::optional<std::string> exact;
    std::optional<std::string> partial;

    for (uint32_t idx : sorted_) {
        const std::string name = to_lower(std::string(entry_name(entries_[idx])));
        if (name == needle) {
            exact = std::string(entry_path(entries_[idx]));
            break;
        }
        if (!partial && name.rfind(needle, 0) == 0) {
            partial = std::string(entry_path(entries_[idx]));
        }
    }

    if (exact) {
        return exact;
    }
    return partial;
}

std::vector<std::string> FileIndex::extract_mentions(std::string_view message) const {
    std::vector<std::string> paths;
    static const std::regex re(R"(@([A-Za-z0-9_./-]+))");
    const std::string text(message);
    for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) {
        const std::string mention = (*it)[1].str();
        if (auto resolved = resolve_mention(mention)) {
            paths.push_back(*resolved);
        }
    }
    return paths;
}

}  // namespace askdocs::agent
