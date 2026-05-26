#pragma once

#include "askdocs/layout.hpp"
#include "askdocs/terminal.hpp"

#include <functional>
#include <string>
#include <vector>

namespace askdocs {

class ExplorerPanel {
public:
    explicit ExplorerPanel(std::string root_path);

    void set_root(std::string path);
    const std::string& root_path() const noexcept { return root_path_; }

    void render(Terminal& term, const Rect& rect, bool focused);
    void mark_all_dirty() noexcept { dirty_all_ = true; }

    void move_up() noexcept;
    void move_down() noexcept;
    void open_or_expand();
    void collapse_or_parent();
    void go_to_parent();
    void toggle_hidden() noexcept;
    void refresh();

    const std::string& selected_path() const;
    bool selected_is_dir() const noexcept;

    using FileOpenCallback = std::function<void(const std::string& path)>;
    void set_on_open(FileOpenCallback cb) { on_open_ = std::move(cb); }

private:
    struct Node {
        std::string name;
        std::string path;
        bool is_dir = false;
        bool expanded = false;
        bool children_loaded = false;
        int depth = 0;
        std::vector<Node> children;
    };

    std::string root_path_;
    Node root_;
    std::vector<Node*> flat_;
    int cursor_ = 0;
    int scroll_ = 0;
    bool show_hidden_ = false;
    bool dirty_all_ = true;
    int last_width_ = -1;
    int last_height_ = -1;
    FileOpenCallback on_open_;

    void load_children(Node& node);
    void rebuild_flat();
    void clamp_scroll(int visible_rows);
    void ensure_cursor_visible(int visible_rows) noexcept;
};

}  // namespace askdocs
