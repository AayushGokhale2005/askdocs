#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace askdocs {

// UTF-8 safe string utilities for terminal rendering.
std::size_t utf8_char_count(std::string_view text) noexcept;
std::size_t utf8_byte_index_for_char(std::string_view text, std::size_t char_index) noexcept;
std::string_view utf8_slice_by_chars(std::string_view text, std::size_t start_char,
                                       std::size_t char_count) noexcept;
std::vector<std::string> wrap_text(std::string_view text, std::size_t max_cols);

// ANSI-aware visible width helpers for terminal output.
std::size_t ansi_visible_char_count(std::string_view text) noexcept;
std::string ansi_clip_visible(std::string_view text, std::size_t max_visible) noexcept;

// OSC 8 hyperlinks (supported by iTerm2, WezTerm, VS Code terminal, etc.).
std::string ansi_hyperlink(std::string_view url, std::string_view label);
std::string style_urls_in_text(std::string_view text, const char* plain_color);

}  // namespace askdocs
