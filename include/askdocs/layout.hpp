#pragma once

#include "askdocs/types.hpp"

namespace askdocs {

struct SplitLayout {
    Rect explorer;
    Rect editor;
    Rect chat;
    Rect status;
    Rect editor_gutter;
    Rect editor_content;
};

SplitLayout compute_layout(const Size& term_size, int explorer_width_ratio = 22,
                           int chat_width_ratio = 34) noexcept;

}  // namespace askdocs
