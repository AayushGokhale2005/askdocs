#include "askdocs/syntax.hpp"
#include "askdocs/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

namespace askdocs {

namespace {

constexpr const char* kReset = "\x1b[0m";
constexpr const char* kKeyword = "\x1b[38;5;141m";
constexpr const char* kType = "\x1b[38;5;117m";
constexpr const char* kString = "\x1b[38;5;114m";
constexpr const char* kComment = "\x1b[90m";
constexpr const char* kNumber = "\x1b[38;5;215m";
constexpr const char* kPreproc = "\x1b[38;5;214m";
constexpr const char* kFunction = "\x1b[38;5;81m";
constexpr const char* kCursorBg = "\x1b[48;5;236m";

enum class TokenKind { Plain, Keyword, Type, String, Comment, Number, Preproc, Function };

struct Token {
    std::size_t start = 0;
    std::size_t end = 0;
    TokenKind kind = TokenKind::Plain;
};

const char* style_for(TokenKind kind) {
    switch (kind) {
        case TokenKind::Keyword:
            return kKeyword;
        case TokenKind::Type:
            return kType;
        case TokenKind::String:
            return kString;
        case TokenKind::Comment:
            return kComment;
        case TokenKind::Number:
            return kNumber;
        case TokenKind::Preproc:
            return kPreproc;
        case TokenKind::Function:
            return kFunction;
        case TokenKind::Plain:
            return kReset;
    }
    return kReset;
}

const std::unordered_set<std::string>& keywords_for(Language lang) {
    static const std::unordered_set<std::string> k_cfamily = {
        "alignas", "alignof", "auto", "break", "case", "catch", "class", "const", "constexpr",
        "continue", "default", "delete", "do", "else", "enum", "explicit", "export", "extern",
        "false", "for", "friend", "goto", "if", "inline", "mutable", "namespace", "new",
        "noexcept", "nullptr", "operator", "private", "protected", "public", "register",
        "return", "sizeof", "static", "struct", "switch", "template", "this", "throw", "true",
        "try", "typedef", "typename", "union", "using", "virtual", "void", "volatile",
        "while", "import", "module", "concept", "requires", "co_await", "co_return",
        "co_yield", "int", "float", "double", "char", "bool", "long", "short", "unsigned",
        "signed", "final", "override", "interface", "implements", "package", "extends",
        "fun", "val", "var", "object", "data", "sealed", "suspend", "lazy", "when", "where",
    };
    static const std::unordered_set<std::string> k_python = {
        "and", "as", "assert", "async", "await", "break", "class", "continue", "def", "del",
        "elif", "else", "except", "False", "finally", "for", "from", "global", "if", "import",
        "in", "is", "lambda", "None", "nonlocal", "not", "or", "pass", "raise", "return",
        "True", "try", "while", "with", "yield", "self",
    };
    static const std::unordered_set<std::string> k_javascript = {
        "async", "await", "break", "case", "catch", "class", "const", "continue", "debugger",
        "default", "delete", "do", "else", "enum", "export", "extends", "false", "finally",
        "for", "function", "if", "import", "in", "instanceof", "let", "new", "null", "return",
        "super", "switch", "this", "throw", "true", "try", "typeof", "undefined", "var",
        "void", "while", "with", "yield", "interface", "type", "implements", "namespace",
        "public", "private", "protected", "readonly", "declare", "module", "from", "as",
    };
    static const std::unordered_set<std::string> k_rust = {
        "as", "async", "await", "break", "const", "continue", "crate", "dyn", "else", "enum",
        "extern", "false", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod",
        "move", "mut", "pub", "ref", "return", "self", "Self", "static", "struct", "super",
        "trait", "true", "type", "unsafe", "use", "where", "while",
    };
    static const std::unordered_set<std::string> k_go = {
        "break", "case", "chan", "const", "continue", "default", "defer", "else", "fallthrough",
        "for", "func", "go", "goto", "if", "import", "interface", "map", "package", "range",
        "return", "select", "struct", "switch", "type", "var",
    };
    static const std::unordered_set<std::string> k_shell = {
        "if", "then", "else", "elif", "fi", "for", "do", "done", "case", "esac", "while",
        "until", "function", "return", "local", "export", "source", "in", "select",
    };
    static const std::unordered_set<std::string> k_sql = {
        "select", "from", "where", "join", "inner", "outer", "left", "right", "on", "group",
        "by", "order", "limit", "insert", "into", "values", "update", "set", "delete", "create",
        "table", "index", "view", "and", "or", "not", "null", "as", "distinct", "having",
        "union", "all", "case", "when", "then", "else", "end", "primary", "key", "foreign",
        "references", "default", "true", "false",
    };
    static const std::unordered_set<std::string> k_generic = {
        "if", "else", "for", "while", "return", "break", "continue", "true", "false", "null",
        "nil", "fn", "func", "def", "let", "var", "const",
    };

    switch (lang) {
        case Language::CFamily:
            return k_cfamily;
        case Language::Python:
            return k_python;
        case Language::JavaScript:
            return k_javascript;
        case Language::Rust:
            return k_rust;
        case Language::Go:
            return k_go;
        case Language::Shell:
            return k_shell;
        case Language::Sql:
            return k_sql;
        default:
            return k_generic;
    }
}

bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool starts_with(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool line_comment_at(Language lang, std::string_view rest, std::string_view& marker) {
    static const std::string_view k_markers[] = {"//", "#", "--", ";"};
    for (const auto& m : k_markers) {
        if (!starts_with(rest, m)) {
            continue;
        }
        if (lang == Language::Python && m == "//") {
            continue;
        }
        if (lang == Language::Shell && m == "//") {
            continue;
        }
        marker = m;
        return true;
    }
    return false;
}

void classify_word(Language lang, std::string_view word, Token& token) {
    const auto& keywords = keywords_for(lang);
    const std::string lower(word);
    if (keywords.count(lower) > 0 || keywords.count(std::string(word)) > 0) {
        token.kind = TokenKind::Keyword;
        return;
    }

    if (lang == Language::CFamily || lang == Language::Rust || lang == Language::Go) {
        static const std::unordered_set<std::string> types = {
            "int", "float", "double", "char", "bool", "long", "short", "unsigned", "signed",
            "string", "size_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t",
            "int16_t", "int32_t", "int64_t", "i8", "i16", "i32", "i64", "u8", "u16", "u32",
            "u64", "f32", "f64", "str", "String", "Vec", "Option", "Result",
        };
        if (types.count(lower) > 0) {
            token.kind = TokenKind::Type;
        }
    }
}

std::vector<Token> tokenize(std::string_view line, Language lang) {
    std::vector<Token> tokens;
    const std::size_t n = line.size();
    std::size_t i = 0;

    while (i < n) {
        const char c = line[i];

        if (lang == Language::CFamily && c == '#') {
            tokens.push_back({i, n, TokenKind::Preproc});
            break;
        }

        std::string_view rest = line.substr(i);
        std::string_view marker;
        if (line_comment_at(lang, rest, marker)) {
            tokens.push_back({i, n, TokenKind::Comment});
            break;
        }

        if (lang != Language::Python && starts_with(rest, "/*")) {
            const std::size_t end = line.find("*/", i + 2);
            tokens.push_back({i, end == std::string_view::npos ? n : end + 2, TokenKind::Comment});
            i = tokens.back().end;
            continue;
        }

        if (lang == Language::Html && starts_with(rest, "<!--")) {
            const std::size_t end = line.find("-->", i + 4);
            tokens.push_back({i, end == std::string_view::npos ? n : end + 3, TokenKind::Comment});
            i = tokens.back().end;
            continue;
        }

        if (c == '"' || c == '\'' || c == '`') {
            const char quote = c;
            std::size_t j = i + 1;
            while (j < n) {
                if (line[j] == '\\' && j + 1 < n) {
                    j += 2;
                    continue;
                }
                if (line[j] == quote) {
                    ++j;
                    break;
                }
                ++j;
            }
            tokens.push_back({i, j, TokenKind::String});
            i = j;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            std::size_t j = i + 1;
            while (j < n && (std::isalnum(static_cast<unsigned char>(line[j])) || line[j] == '.' ||
                             line[j] == 'x' || line[j] == 'X' || line[j] == '_')) {
                ++j;
            }
            tokens.push_back({i, j, TokenKind::Number});
            i = j;
            continue;
        }

        if (is_ident_start(c)) {
            std::size_t j = i + 1;
            while (j < n && is_ident_char(line[j])) {
                ++j;
            }
            Token token{i, j, TokenKind::Plain};
            classify_word(lang, line.substr(i, j - i), token);
            if (j < n && line[j] == '(') {
                token.kind = TokenKind::Function;
            }
            tokens.push_back(token);
            i = j;
            continue;
        }

        ++i;
    }

    return tokens;
}

std::string render_tokens(std::string_view line, const std::vector<Token>& tokens, int max_cols,
                          int cursor_col, bool show_cursor) {
    std::string out;
    const std::size_t max_bytes =
        utf8_byte_index_for_char(line, static_cast<std::size_t>(std::max(0, max_cols)));

    auto emit_plain = [&](std::size_t start, std::size_t end) {
        if (start >= end || start >= max_bytes) {
            return;
        }
        end = std::min(end, max_bytes);
        out.append(line.substr(start, end - start));
    };

    if (tokens.empty()) {
        emit_plain(0, line.size());
    } else {
        std::size_t pos = 0;
        for (const Token& token : tokens) {
            emit_plain(pos, token.start);
            if (token.start < max_bytes) {
                const std::size_t end = std::min(token.end, max_bytes);
                out.append(style_for(token.kind));
                out.append(line.substr(token.start, end - token.start));
                out.append(kReset);
            }
            pos = token.end;
        }
        emit_plain(pos, line.size());
    }

    if (show_cursor) {
        const std::size_t cursor_byte =
            utf8_byte_index_for_char(line, static_cast<std::size_t>(std::max(0, cursor_col)));
        // Re-render with cursor overlay is handled by a simpler path below when needed.
        (void)cursor_byte;
    }

    return out;
}

std::string render_with_cursor(std::string_view line, Language lang, int cursor_col, int max_cols) {
    const auto tokens = tokenize(line, lang);
    std::string out;

    const int total_chars = static_cast<int>(utf8_char_count(line));
    const int safe_cursor = std::max(0, std::min(cursor_col, total_chars));
    const std::size_t before_bytes =
        utf8_byte_index_for_char(line, static_cast<std::size_t>(safe_cursor));
    const std::size_t at_bytes =
        utf8_byte_index_for_char(line, static_cast<std::size_t>(safe_cursor + 1));
    const std::size_t max_bytes =
        utf8_byte_index_for_char(line, static_cast<std::size_t>(std::max(1, max_cols)));

    auto styled_segment = [&](std::size_t start, std::size_t end) -> std::string {
        if (start >= end || start >= max_bytes) {
            return {};
        }
        end = std::min(end, max_bytes);
        TokenKind kind = TokenKind::Plain;
        for (const Token& token : tokens) {
            if (start >= token.start && start < token.end) {
                kind = token.kind;
                break;
            }
        }
        std::string seg;
        seg.append(style_for(kind));
        seg.append(line.substr(start, end - start));
        seg.append(kReset);
        return seg;
    };

    out.append(styled_segment(0, before_bytes));
    out.append(kCursorBg);
    if (before_bytes >= line.size() || at_bytes <= before_bytes) {
        out.push_back(' ');
    } else {
        out.append(line.substr(before_bytes, at_bytes - before_bytes));
    }
    out.append(kReset);
    out.append(styled_segment(at_bytes, max_bytes));
    return out;
}

}  // namespace

const char* language_name(Language lang) noexcept {
    switch (lang) {
        case Language::Generic:
            return "generic";
        case Language::CFamily:
            return "c-family";
        case Language::Python:
            return "python";
        case Language::JavaScript:
            return "javascript";
        case Language::Rust:
            return "rust";
        case Language::Go:
            return "go";
        case Language::Shell:
            return "shell";
        case Language::Markdown:
            return "markdown";
        case Language::Json:
            return "json";
        case Language::Yaml:
            return "yaml";
        case Language::Html:
            return "html";
        case Language::Css:
            return "css";
        case Language::Sql:
            return "sql";
    }
    return "generic";
}

std::string highlight_line(std::string_view line, Language lang, int max_display_cols) {
    const auto tokens = tokenize(line, lang);
    return render_tokens(line, tokens, max_display_cols, -1, false);
}

std::string highlight_line_with_cursor(std::string_view line, Language lang, int cursor_col,
                                       int max_display_cols, bool show_cursor) {
    if (!show_cursor) {
        return highlight_line(line, lang, max_display_cols);
    }
    return render_with_cursor(line, lang, cursor_col, max_display_cols);
}

}  // namespace askdocs
