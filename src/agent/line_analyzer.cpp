#include "askdocs/agent/line_analyzer.hpp"

#include "askdocs/file_io.hpp"
#include "askdocs/docs/online_search.hpp"
#include "askdocs/nlp/tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace askdocs::agent {

namespace {

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string strip_comments(std::string_view line) {
    std::string out(line);
    const std::size_t slash = out.find("//");
    if (slash != std::string::npos) {
        out.erase(slash);
    }
    const std::size_t hash = out.find('#');
    if (hash != std::string::npos) {
        out.erase(hash);
    }
    return trim(out);
}

std::string join_symbols(const std::vector<std::string>& symbols) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << symbols[i];
    }
    return oss.str();
}

}  // namespace

bool query_targets_specific_line(std::string_view query) {
    return parse_line_reference(query).has_value();
}

std::optional<LineReference> parse_line_reference(std::string_view query) {
    static const std::regex patterns[] = {
        std::regex(R"((?:what\s+(?:does|is|do)\s+)?line\s+#?(\d+))", std::regex::icase),
        std::regex(R"(\bl(?:ine)?\s*#?(\d+)\b)", std::regex::icase),
        std::regex(R"(\@?\w+\.(?:cpp|c|h|py|js)[:\s]+(?:line\s*)?#?(\d+))", std::regex::icase),
    };

    const std::string text(query);
    for (const auto& re : patterns) {
        std::smatch m;
        if (std::regex_search(text, m, re) && m.size() >= 2) {
            const int line = std::stoi(m[1].str());
            if (line > 0) {
                return LineReference{line};
            }
        }
    }
    return std::nullopt;
}

LineSnapshot fetch_line_snapshot(const EditorState& editor, ContextBuffer& buffer,
                                 std::string_view filepath, int line_number,
                                 int context_radius) {
    LineSnapshot snap;
    snap.ref.line_number = line_number;
    snap.filepath = filepath.empty() ? editor.filepath : std::string(filepath);

    if (snap.filepath.empty() || snap.filepath == "untitled") {
        return snap;
    }

    const int idx = line_number - 1;
    std::vector<std::string> lines;

    if (editor.filepath == snap.filepath && idx >= 0 &&
        idx < static_cast<int>(editor.lines.size())) {
        lines = editor.lines;
    } else if (auto slice = buffer.get(snap.filepath, idx)) {
        const int local = idx - slice->start_line;
        if (local >= 0 && local < static_cast<int>(slice->lines.size())) {
            snap.line_text = trim(slice->lines[static_cast<std::size_t>(local)]);
            const int start = std::max(0, local - context_radius);
            const int end = std::min(static_cast<int>(slice->lines.size()), local + context_radius + 1);
            for (int i = start; i < end; ++i) {
                snap.context.push_back(
                    {slice->start_line + i + 1, trim(slice->lines[static_cast<std::size_t>(i)])});
            }
            snap.valid = true;
            return snap;
        }
    }

    if (lines.empty()) {
        const LoadedFile loaded = load_file(snap.filepath);
        if (!loaded.ok) {
            return snap;
        }
        lines = loaded.lines;
    }

    if (idx < 0 || idx >= static_cast<int>(lines.size())) {
        return snap;
    }

    snap.line_text = trim(lines[static_cast<std::size_t>(idx)]);
    const int start = std::max(0, idx - context_radius);
    const int end = std::min(static_cast<int>(lines.size()), idx + context_radius + 1);
    for (int i = start; i < end; ++i) {
        snap.context.push_back({i + 1, trim(lines[static_cast<std::size_t>(i)])});
    }
    snap.valid = true;
    return snap;
}

std::vector<std::string> extract_symbols(std::string_view line, Language lang) {
    std::vector<std::string> symbols;
    const std::string cleaned = strip_comments(line);

    for (const std::string& tok : nlp::tokenize(cleaned)) {
        if (tok.size() >= 2) {
            symbols.push_back(tok);
        }
    }

    static const char* k_cpp[] = {"for",  "while", "if",    "return", "class", "struct",
                                  "new",  "delete", "vector", "std",   "include"};
    static const char* k_py[] = {"for", "while", "if", "return", "def", "class", "import", "range", "in"};

    const char** keys = k_cpp;
    std::size_t key_count = sizeof(k_cpp) / sizeof(k_cpp[0]);
    if (lang == Language::Python) {
        keys = k_py;
        key_count = sizeof(k_py) / sizeof(k_py[0]);
    }

    std::string lower = cleaned;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    for (std::size_t i = 0; i < key_count; ++i) {
        if (lower.find(keys[i]) != std::string::npos) {
            symbols.push_back(keys[i]);
        }
    }

    if (cleaned.find("->") != std::string::npos) {
        symbols.push_back("pointer");
    }
    if (cleaned.find("::") != std::string::npos) {
        symbols.push_back("scope");
    }
    if (cleaned.find("+=") != std::string::npos || cleaned.find("-=") != std::string::npos) {
        symbols.push_back("assignment");
    }

    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
    return symbols;
}

std::vector<docs::OnlineDocHit> cross_reference_online(const LineSnapshot& snap,
                                                       std::string_view lang) {
    std::ostringstream query;
    query << snap.line_text;
    for (const std::string& sym : snap.symbols) {
        query << ' ' << sym;
    }
    return docs::search_online_docs(query.str(), lang, 3);
}

std::string explain_line(const LineSnapshot& snap,
                         const std::vector<docs::OnlineDocHit>& online_hits) {
    std::ostringstream out;

    out << "Line " << snap.ref.line_number;
    if (!snap.filepath.empty()) {
        out << " in " << snap.filepath;
    }
    out << ":\n";

    for (const auto& ctx : snap.context) {
        out << (ctx.first == snap.ref.line_number ? "> " : "  ") << ctx.first << " | " << ctx.second
            << '\n';
    }

    out << "\n";

    const std::string line = strip_comments(snap.line_text);
    std::string lower = line;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (line.empty() || line == "{" || line == "}" || line == "{}") {
        out << "This is a structural line — it delimits a block of code.";
    } else if (lower.find("#include") != std::string::npos ||
               lower.find("import ") != std::string::npos) {
        out << "This pulls in external code used elsewhere in the file.";
    } else if (lower.find("for ") != std::string::npos || lower.find("while ") != std::string::npos) {
        out << "This is a loop — the body repeats while the condition or iterator allows.";
    } else if (lower.find("if ") != std::string::npos || lower.find("else") != std::string::npos) {
        out << "This is conditional control flow — it runs only when the condition is true.";
    } else if (lower.find("return") != std::string::npos) {
        out << "This exits the current function and returns a value to the caller.";
    } else if (line.find("+=") != std::string::npos) {
        out << "This adds to a variable and stores the result (compound assignment).";
    } else if (line.find("-=") != std::string::npos) {
        out << "This subtracts from a variable and stores the result.";
    } else if (line.find("->") != std::string::npos) {
        out << "This accesses a member through a pointer.";
    } else if (lower.find("class ") != std::string::npos || lower.find("struct ") != std::string::npos) {
        out << "This defines a type — data plus behavior with constructor invariants.";
    } else if (lower.find("def ") != std::string::npos) {
        out << "This defines a function — parameters in, return value or side effect out.";
    } else if (line.find('=') != std::string::npos && line.find("==") == std::string::npos &&
               line.find("!=") == std::string::npos && line.find("<=") == std::string::npos &&
               line.find(">=") == std::string::npos) {
        out << "This assigns or initializes a value.";
    } else if (lower.find("(") != std::string::npos && lower.find(")") != std::string::npos) {
        out << "This invokes or declares a function — check its return value and side effects.";
    } else {
        out << "Treat this as a statement: what state changes when it runs?";
    }

    if (!snap.symbols.empty()) {
        out << "\n\nsymbols: " << join_symbols(snap.symbols);
    }

    if (!online_hits.empty()) {
        out << "\n\nDocs — " << online_hits.front().title << ":\n";
        out << online_hits.front().url;
        if (online_hits.size() > 1) {
            out << "\n\nsee also:\n";
            for (std::size_t i = 1; i < online_hits.size() && i < 3; ++i) {
                out << "· " << online_hits[i].title << "\n  " << online_hits[i].url << '\n';
            }
        }
    }

    return out.str();
}

}  // namespace askdocs::agent
