#include "askdocs/layout.hpp"

#include <algorithm>

namespace askdocs {

SplitLayout compute_layout(const Size& term_size, int explorer_width_ratio,
                           int chat_width_ratio) noexcept {
    SplitLayout layout{};

    const int cols = term_size.cols > 0 ? term_size.cols : 80;
    const int rows = term_size.rows > 0 ? term_size.rows : 24;
    const int status_height = 1;
    const int usable_rows = std::max(1, rows - status_height);

    int explorer_width = std::clamp(cols * explorer_width_ratio / 100, 12, 36);
    int chat_width = std::clamp(cols * chat_width_ratio / 100, 16, 40);
    const int separators = 2;
    const int min_editor = 14;

    auto total_width = [&]() { return explorer_width + chat_width + min_editor + separators; };

    while (total_width() > cols && (explorer_width > 10 || chat_width > 14)) {
        if (explorer_width >= chat_width && explorer_width > 10) {
            --explorer_width;
        } else if (chat_width > 14) {
            --chat_width;
        } else {
            --explorer_width;
        }
    }

    int editor_width = std::max(10, cols - explorer_width - chat_width - separators);
    if (explorer_width + chat_width + editor_width + separators > cols) {
        chat_width = std::max(14, cols - explorer_width - editor_width - separators);
        editor_width = std::max(10, cols - explorer_width - chat_width - separators);
    }

    layout.explorer = Rect{0, 0, usable_rows, explorer_width};
    layout.editor = Rect{0, explorer_width + 1, usable_rows, editor_width};
    layout.chat = Rect{0, explorer_width + 1 + editor_width + 1, usable_rows, chat_width};
    layout.status = Rect{rows - status_height, 0, status_height, cols};

    const int gutter = std::min(6, std::max(4, editor_width / 8));
    layout.editor_gutter = Rect{
        layout.editor.top,
        layout.editor.left,
        layout.editor.height,
        gutter,
    };
    layout.editor_content = Rect{
        layout.editor.top,
        layout.editor.left + gutter,
        layout.editor.height,
        std::max(1, layout.editor.width - gutter),
    };

    return layout;
}

}  // namespace askdocs
