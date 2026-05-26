#include "askdocs/editor_view.hpp"
#include "askdocs/syntax.hpp"
#include "askdocs/utf8.hpp"

#include <algorithm>
#include <string>

namespace askdocs {

namespace {

constexpr const char* kFocusBorder = "\x1b[38;5;39m";
constexpr const char* kDimBorder = "\x1b[90m";
constexpr const char* kReset = "\x1b[0m";
constexpr const char* kLineNum = "\x1b[90m";

int visible_line_count(const Rect& content) { return std::max(0, content.height - 2); }

}  // namespace

void EditorView::render(Terminal& term, const SplitLayout& layout, bool focused) {
    if (!state_) {
        return;
    }

    const bool geometry_changed = layout.editor_content.width != last_width_ ||
                                  layout.editor_content.height != last_height_ ||
                                  state_->scroll_row != last_scroll_;
    if (dirty_all_ || geometry_changed) {
        term.clear_rect(layout.editor);
        dirty_all_ = false;
        last_width_ = layout.editor_content.width;
        last_height_ = layout.editor_content.height;
        last_scroll_ = state_->scroll_row;
    }

    const char* border = focused ? kFocusBorder : kDimBorder;

    term.move_cursor(layout.editor.top, layout.editor.left + layout.editor.width);
    term.write(border);
    term.write("│");
    term.write(kReset);
    for (int r = 0; r < layout.editor.height; ++r) {
        term.move_cursor(layout.editor.top + r, layout.editor.left + layout.editor.width);
        term.write(border);
        term.write("│");
        term.write(kReset);
    }

    term.move_cursor(layout.editor.top, layout.editor.left);
    term.write(border);
    term.write("┌");
    for (int i = 1; i < layout.editor.width; ++i) {
        term.write("─");
    }
    term.write("┐");
    term.write(kReset);

    term.move_cursor(layout.editor.top + layout.editor.height - 1, layout.editor.left);
    term.write(border);
    term.write("└");
    for (int i = 1; i < layout.editor.width; ++i) {
        term.write("─");
    }
    term.write("┘");
    term.write(kReset);

    const int visible = visible_line_count(layout.editor_content);
    for (int screen_row = 0; screen_row < visible; ++screen_row) {
        const int buf_row = state_->scroll_row + screen_row;
        draw_line(term, layout.editor_content.top + 1 + screen_row, layout.editor_gutter,
                  layout.editor_content, buf_row, focused);
    }

    if (focused) {
        term.hide_cursor();
    }
}

void EditorView::draw_line(Terminal& term, int screen_row, const Rect& gutter, const Rect& content,
                           int buf_row, bool focused) const {
    // Write line number into gutter (erase only from gutter.left rightward on this row,
    // but cap output to gutter width so we don't bleed into the content area).
    term.move_cursor(screen_row, gutter.left);
    term.write(kLineNum);
    const std::string num = std::to_string(buf_row + 1);
    // Pad/clip to fill gutter width exactly so the content column is clean.
    std::string padded = ansi_clip_visible(num, static_cast<std::size_t>(gutter.width));
    while (static_cast<int>(padded.size()) < gutter.width) { padded += ' '; }
    term.write(padded);
    term.write(kReset);

    // Erase from content.left to end-of-line (not the whole line — that would
    // destroy the explorer on the left).
    term.move_cursor(screen_row, content.left);
    term.write("\x1b[K");

    std::string line;
    if (buf_row >= 0 && buf_row < static_cast<int>(state_->lines.size())) {
        line = state_->lines[static_cast<std::size_t>(buf_row)];
    }

    const int max_chars = std::max(1, content.width);
    const bool on_cursor_line = focused && buf_row == state_->cursor.row;

    const std::string rendered = highlight_line_with_cursor(
        line, state_->language, state_->cursor.col, max_chars, on_cursor_line);
    term.write(ansi_clip_visible(rendered, static_cast<std::size_t>(max_chars)));
}

}  // namespace askdocs
