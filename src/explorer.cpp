#include "askdocs/explorer.hpp"
#include "askdocs/utf8.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <system_error>

namespace fs = std::filesystem;

namespace askdocs {

namespace {

constexpr const char* kReset = "\x1b[0m";
constexpr const char* kTitle = "\x1b[1;38;5;109m";
constexpr const char* kFocus = "\x1b[48;5;236m";
constexpr const char* kDim = "\x1b[90m";
constexpr const char* kDir = "\x1b[38;5;117m";
constexpr const char* kFile = "\x1b[38;5;252m";
constexpr const char* kSelectedDir = "\x1b[1;38;5;81m";
constexpr const char* kSelectedFile = "\x1b[1;38;5;229m";

std::string basename(const std::string& path) {
    return fs::path(path).filename().string();
}

bool is_hidden(const std::string& name, bool show_hidden) {
    if (show_hidden) {
        return false;
    }
    return !name.empty() && name[0] == '.';
}

}  // namespace

ExplorerPanel::ExplorerPanel(std::string root_path) : root_path_(std::move(root_path)) {
    root_.name = basename(root_path_);
    root_.path = root_path_;
    root_.is_dir = true;
    root_.expanded = true;
    root_.depth = 0;
    load_children(root_);
    rebuild_flat();
}

void ExplorerPanel::set_root(std::string path) {
    root_path_ = std::move(path);
    root_ = Node{};
    root_.name = basename(root_path_);
    root_.path = root_path_;
    root_.is_dir = true;
    root_.expanded = true;
    root_.depth = 0;
    load_children(root_);
    cursor_ = 0;
    scroll_ = 0;
    rebuild_flat();
    mark_all_dirty();
}

void ExplorerPanel::load_children(Node& node) {
    node.children.clear();
    node.children_loaded = true;

    std::error_code ec;
    if (!fs::is_directory(node.path, ec)) {
        return;
    }

    std::vector<fs::directory_entry> entries;
    for (const auto& entry : fs::directory_iterator(node.path, ec)) {
        if (ec) {
            break;
        }
        const std::string name = entry.path().filename().string();
        if (is_hidden(name, show_hidden_)) {
            continue;
        }
        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
        std::error_code ec_a;
        std::error_code ec_b;
        const bool dir_a = a.is_directory(ec_a);
        const bool dir_b = b.is_directory(ec_b);
        if (dir_a != dir_b) {
            return dir_a > dir_b;
        }
        return a.path().filename().string() < b.path().filename().string();
    });

    for (const auto& entry : entries) {
        Node child;
        child.name = entry.path().filename().string();
        child.path = entry.path().string();
        child.is_dir = entry.is_directory(ec);
        child.depth = node.depth + 1;
        node.children.push_back(std::move(child));
    }
}

void ExplorerPanel::rebuild_flat() {
    flat_.clear();
    flat_.push_back(&root_);
    if (!root_.expanded) {
        return;
    }

    std::function<void(Node&)> walk = [&](Node& node) {
        for (Node& child : node.children) {
            flat_.push_back(&child);
            if (child.is_dir && child.expanded) {
                if (!child.children_loaded) {
                    load_children(child);
                }
                walk(child);
            }
        }
    };
    walk(root_);
}

void ExplorerPanel::move_up() noexcept {
    if (cursor_ > 0) {
        --cursor_;
        dirty_all_ = true;
    }
}

void ExplorerPanel::move_down() noexcept {
    if (!flat_.empty() && cursor_ + 1 < static_cast<int>(flat_.size())) {
        ++cursor_;
        dirty_all_ = true;
    }
}

void ExplorerPanel::open_or_expand() {
    if (flat_.empty()) {
        return;
    }

    Node& node = *flat_[static_cast<std::size_t>(cursor_)];
    if (node.is_dir) {
        node.expanded = !node.expanded;
        if (node.expanded && !node.children_loaded) {
            load_children(node);
        }
        rebuild_flat();
    } else if (on_open_) {
        on_open_(node.path);
    }
    dirty_all_ = true;
}

void ExplorerPanel::collapse_or_parent() {
    if (flat_.empty()) {
        return;
    }

    Node& node = *flat_[static_cast<std::size_t>(cursor_)];
    if (node.is_dir && node.expanded) {
        node.expanded = false;
        rebuild_flat();
    } else {
        go_to_parent();
    }
    dirty_all_ = true;
}

void ExplorerPanel::go_to_parent() {
    std::error_code ec;
    const fs::path parent = fs::path(root_path_).parent_path();
    if (parent.empty() || !fs::exists(parent, ec)) {
        return;
    }
    set_root(parent.string());
}

void ExplorerPanel::toggle_hidden() noexcept {
    show_hidden_ = !show_hidden_;
    root_.children_loaded = false;
    load_children(root_);
    for (Node& child : root_.children) {
        if (child.is_dir && child.expanded) {
            child.children_loaded = false;
            load_children(child);
        }
    }
    rebuild_flat();
    dirty_all_ = true;
}

void ExplorerPanel::refresh() {
    root_.children_loaded = false;
    load_children(root_);
    for (Node& child : root_.children) {
        if (child.is_dir && child.expanded) {
            child.children_loaded = false;
            load_children(child);
        }
    }
    rebuild_flat();
    dirty_all_ = true;
}

const std::string& ExplorerPanel::selected_path() const {
    static const std::string kEmpty;
    if (flat_.empty()) {
        return kEmpty;
    }
    return flat_[static_cast<std::size_t>(cursor_)]->path;
}

bool ExplorerPanel::selected_is_dir() const noexcept {
    if (flat_.empty()) {
        return false;
    }
    return flat_[static_cast<std::size_t>(cursor_)]->is_dir;
}

void ExplorerPanel::clamp_scroll(int visible_rows) {
    if (visible_rows <= 0) {
        return;
    }
    const int max_scroll = std::max(0, static_cast<int>(flat_.size()) - visible_rows);
    scroll_ = std::clamp(scroll_, 0, max_scroll);
    cursor_ = std::clamp(cursor_, 0, std::max(0, static_cast<int>(flat_.size()) - 1));
}

void ExplorerPanel::ensure_cursor_visible(int visible_rows) noexcept {
    clamp_scroll(visible_rows);
    if (cursor_ < scroll_) {
        scroll_ = cursor_;
    } else if (cursor_ >= scroll_ + visible_rows) {
        scroll_ = cursor_ - visible_rows + 1;
    }
}

void ExplorerPanel::render(Terminal& term, const Rect& rect, bool focused) {
    const bool geometry_changed = rect.width != last_width_ || rect.height != last_height_;
    if (dirty_all_ || geometry_changed) {
        term.clear_rect(rect);
        dirty_all_ = false;
        last_width_ = rect.width;
        last_height_ = rect.height;
    }

    const int inner_width = std::max(1, rect.width - 2);
    const int header_rows = 2;
    const int body_rows = std::max(1, rect.height - header_rows - 1);
    ensure_cursor_visible(body_rows);

    term.move_cursor(rect.top, rect.left);
    term.write(kTitle);
    term.write(" Files ");
    term.write(kDim);
    term.write(kReset);

    term.move_cursor(rect.top + 1, rect.left);
    term.write(kDim);
    std::string path_display = root_path_;
    if (static_cast<int>(utf8_char_count(path_display)) > inner_width) {
        const std::size_t start = path_display.size() >
                                          static_cast<std::size_t>(std::max(0, inner_width - 1))
                                      ? path_display.size() - static_cast<std::size_t>(inner_width - 1)
                                      : 0;
        path_display = "…" + path_display.substr(start);
    }
    term.write(utf8_slice_by_chars(path_display, 0, static_cast<std::size_t>(inner_width)));
    term.write(kReset);

    for (int row = 0; row < body_rows; ++row) {
        const int idx = scroll_ + row;
        term.move_cursor(rect.top + header_rows + row, rect.left);
        term.write("\x1b[K");

        if (idx < 0 || idx >= static_cast<int>(flat_.size())) {
            continue;
        }

        const Node& node = *flat_[static_cast<std::size_t>(idx)];
        const bool selected = idx == cursor_;
        const char* style = node.is_dir ? (selected ? kSelectedDir : kDir) : (selected ? kSelectedFile : kFile);

        if (selected && focused) {
            term.write(kFocus);
        }

        std::string prefix;
        prefix.append(static_cast<std::size_t>(node.depth * 2), ' ');
        if (node.is_dir) {
            prefix += node.expanded ? "▾ " : "▸ ";
        } else {
            prefix += "  ";
        }

        std::string label = prefix + node.name;
        if (node.is_dir) {
            label += "/";
        }

        term.write(style);
        term.write(utf8_slice_by_chars(label, 0, static_cast<std::size_t>(inner_width)));
        term.write(kReset);
    }

    term.move_cursor(rect.top + rect.height - 1, rect.left);
    term.write(kDim);
    term.write("Enter open  \xe2\x86\x90 collapse  - up  a hidden  r refresh");
    term.write(kReset);
}

}  // namespace askdocs
