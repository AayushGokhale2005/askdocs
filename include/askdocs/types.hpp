#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace askdocs {

struct Size {
    int rows = 0;
    int cols = 0;
};

struct Point {
    int row = 0;
    int col = 0;
};

struct Rect {
    int top = 0;
    int left = 0;
    int height = 0;
    int width = 0;

    bool contains(int row, int col) const noexcept;
};

enum class FocusTarget { Explorer, Editor, Chat };

enum class Language {
    Generic,
    CFamily,
    Python,
    JavaScript,
    Rust,
    Go,
    Shell,
    Markdown,
    Json,
    Yaml,
    Html,
    Css,
    Sql,
};

enum class AiMode {
    Learn,
    Debug,
    Explain,
    Quiz,
    Override,
};

enum class MessageRole { User, Tutor, System, Agent };

struct ChatMessage {
    MessageRole role = MessageRole::System;
    std::string text;
    AiMode mode = AiMode::Learn;
};

struct EditorState {
    std::vector<std::string> lines;
    Point cursor{0, 0};
    Point selection_anchor{-1, -1};
    int scroll_row = 0;
    std::string filepath = "untitled";
    Language language = Language::Generic;
    bool dirty = false;
};

}  // namespace askdocs
