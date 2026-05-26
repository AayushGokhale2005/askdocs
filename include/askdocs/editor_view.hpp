#pragma once

#include "askdocs/layout.hpp"
#include "askdocs/terminal.hpp"
#include "askdocs/types.hpp"

#include <vector>

namespace askdocs {

class EditorView {
public:
    void set_state(const EditorState& state) noexcept { state_ = &state; }

    void render(Terminal& term, const SplitLayout& layout, bool focused);
    void mark_all_dirty() noexcept { dirty_all_ = true; }

private:
    const EditorState* state_ = nullptr;
    bool dirty_all_ = true;
    int last_scroll_ = -1;
    int last_width_ = -1;
    int last_height_ = -1;

    void draw_line(Terminal& term, int screen_row, const Rect& gutter, const Rect& content,
                   int buf_row, bool focused) const;
};

}  // namespace askdocs
