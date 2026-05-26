#pragma once

#include "askdocs/types.hpp"

#include <string>
#include <string_view>

namespace askdocs {

std::string highlight_line(std::string_view line, Language lang, int max_display_cols);
std::string highlight_line_with_cursor(std::string_view line, Language lang, int cursor_col,
                                       int max_display_cols, bool show_cursor);

const char* language_name(Language lang) noexcept;

}  // namespace askdocs
