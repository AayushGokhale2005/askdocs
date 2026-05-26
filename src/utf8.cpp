#include "askdocs/utf8.hpp"

#include <cctype>
#include <cstdint>

namespace askdocs {

namespace {

bool utf8_lead_byte(unsigned char b) noexcept { return (b & 0xC0) != 0x80; }

bool ansi_csi_final(unsigned char b) noexcept { return b >= 0x40 && b <= 0x7E; }

std::size_t skip_ansi_sequence(std::string_view text, std::size_t index) noexcept {
    if (index >= text.size() || text[index] != '\x1b') {
        return index + 1;
    }
    if (index + 1 < text.size() && text[index + 1] == ']') {
        // OSC sequence (hyperlinks, window title): ESC ] payload, terminated by BEL or ST.
        std::size_t j = index + 2;
        while (j < text.size()) {
            if (text[j] == '\x07') {
                return j + 1;
            }
            if (text[j] == '\x1b' && j + 1 < text.size() && text[j + 1] == '\\') {
                return j + 2;
            }
            ++j;
        }
        return text.size();
    }
    if (index + 1 < text.size() && text[index + 1] == '[') {
        std::size_t j = index + 2;
        while (j < text.size() && !ansi_csi_final(static_cast<unsigned char>(text[j]))) {
            ++j;
        }
        if (j < text.size()) {
            return j + 1;
        }
    }
    return index + 1;
}

std::size_t utf8_char_byte_length(unsigned char lead) noexcept {
    if ((lead & 0x80) == 0) {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0) {
        return 2;
    }
    if ((lead & 0xF0) == 0xE0) {
        return 3;
    }
    if ((lead & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

}  // namespace

std::size_t utf8_char_count(std::string_view text) noexcept {
    std::size_t count = 0;
    for (unsigned char b : text) {
        if (utf8_lead_byte(b)) {
            ++count;
        }
    }
    return count;
}

std::size_t utf8_byte_index_for_char(std::string_view text, std::size_t char_index) noexcept {
    std::size_t idx = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (utf8_lead_byte(static_cast<unsigned char>(text[i]))) {
            if (idx == char_index) {
                return i;
            }
            ++idx;
        }
    }
    return text.size();
}

std::string_view utf8_slice_by_chars(std::string_view text, std::size_t start_char,
                                     std::size_t char_count) noexcept {
    const std::size_t start = utf8_byte_index_for_char(text, start_char);
    const std::size_t end = utf8_byte_index_for_char(text, start_char + char_count);
    return text.substr(start, end - start);
}

std::vector<std::string> wrap_text(std::string_view text, std::size_t max_cols) {
    std::vector<std::string> lines;
    if (max_cols == 0) {
        lines.emplace_back(text);
        return lines;
    }

    std::size_t pos = 0;
    while (pos < text.size()) {
        if (text[pos] == '\n') {
            lines.emplace_back();
            ++pos;
            continue;
        }

        std::size_t line_start = pos;
        std::size_t line_chars = 0;
        std::size_t last_break = std::string_view::npos;

        while (pos < text.size() && text[pos] != '\n') {
            const unsigned char b = static_cast<unsigned char>(text[pos]);
            if (utf8_lead_byte(b)) {
                if (line_chars + 1 > max_cols) {
                    break;
                }
                if (text[pos] == ' ') {
                    last_break = pos;
                }
                ++line_chars;
            }
            ++pos;
        }

        if (line_chars > max_cols || (pos < text.size() && text[pos] != '\n' && line_chars == max_cols)) {
            if (last_break != std::string_view::npos && last_break > line_start) {
                lines.emplace_back(text.substr(line_start, last_break - line_start));
                pos = last_break + 1;
                while (pos < text.size() && text[pos] == ' ') {
                    ++pos;
                }
                continue;
            }
        }

        lines.emplace_back(text.substr(line_start, pos - line_start));
    }

    if (lines.empty()) {
        lines.emplace_back();
    }
    return lines;
}

std::size_t ansi_visible_char_count(std::string_view text) noexcept {
    std::size_t visible = 0;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] == '\x1b') {
            i = skip_ansi_sequence(text, i);
            continue;
        }
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        if (!utf8_lead_byte(lead)) {
            ++i;
            continue;
        }
        ++visible;
        i += utf8_char_byte_length(lead);
    }
    return visible;
}

std::string ansi_clip_visible(std::string_view text, std::size_t max_visible) noexcept {
    std::string out;
    std::size_t visible = 0;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] == '\x1b') {
            const std::size_t next = skip_ansi_sequence(text, i);
            out.append(text.substr(i, next - i));
            i = next;
            continue;
        }
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        if (!utf8_lead_byte(lead)) {
            ++i;
            continue;
        }
        const std::size_t len = utf8_char_byte_length(lead);
        if (visible >= max_visible) {
            break;
        }
        out.append(text.substr(i, len));
        ++visible;
        i += len;
    }
    if (!out.empty()) {
        out.append("\x1b[0m");
    }
    return out;
}

std::string ansi_hyperlink(std::string_view url, std::string_view label) {
    std::string out;
    out.append("\x1b]8;;");
    out.append(url);
    out.append("\x1b\\");
    out.append("\x1b[38;5;39m");
    out.append(label);
    out.append("\x1b]8;;\x1b\\");
    return out;
}

namespace {

bool url_char(unsigned char c) noexcept {
    return !std::isspace(c) && c != ')' && c != ']' && c != '>' && c != '"';
}

}  // namespace

std::string style_urls_in_text(std::string_view text, const char* plain_color) {
    constexpr const char* kReset = "\x1b[0m";

    std::string out;
    out.reserve(text.size() + 64);
    std::size_t i = 0;

    while (i < text.size()) {
        const bool https = i + 8 <= text.size() && text.compare(i, 8, "https://") == 0;
        const bool http = !https && i + 7 <= text.size() && text.compare(i, 7, "http://") == 0;
        if (https || http) {
            std::size_t j = i;
            while (j < text.size() && url_char(static_cast<unsigned char>(text[j]))) {
                ++j;
            }
            const std::string_view url = text.substr(i, j - i);
            out.append(ansi_hyperlink(url, url));
            out.append(kReset);
            out.append(plain_color);
            i = j;
            continue;
        }

        const std::size_t char_idx = utf8_char_count(text.substr(0, i));
        const std::string_view glyph = utf8_slice_by_chars(text, char_idx, 1);
        out.append(plain_color);
        out.append(glyph);
        i += glyph.size();
    }

    return out;
}

}  // namespace askdocs
