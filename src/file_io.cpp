#include "askdocs/file_io.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace askdocs {

namespace {

std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool ext_in(std::string_view ext, std::initializer_list<const char*> list) {
    for (const char* item : list) {
        if (ext == item) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> split_lines(const std::string& content) {
    std::vector<std::string> lines;
    std::string line;
    std::istringstream stream(content);
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    if (lines.empty()) {
        lines.emplace_back();
    }
    return lines;
}

bool ends_with(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

std::string get_working_directory() {
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (ec) {
        return ".";
    }
    return cwd.string();
}

std::string resolve_path(const std::string& base, const std::string& relative) {
    std::error_code ec;
    fs::path resolved = fs::weakly_canonical(fs::path(base) / relative, ec);
    if (ec) {
        return (fs::path(base) / relative).string();
    }
    return resolved.string();
}

std::string extension_from_path(std::string_view path) {
    const std::size_t dot = path.find_last_of('.');
    const std::size_t slash = path.find_last_of("/\\");
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
        return {};
    }
    return to_lower(std::string(path.substr(dot + 1)));
}

Language language_from_extension(std::string_view ext) noexcept {
    if (ext_in(ext, {"cpp", "cxx", "cc", "c", "h", "hpp", "hh", "hxx", "ino"})) {
        return Language::CFamily;
    }
    if (ext_in(ext, {"java", "cs", "kt", "kts", "scala", "swift", "m", "mm"})) {
        return Language::CFamily;
    }
    if (ext_in(ext, {"py", "pyw", "pyi"})) {
        return Language::Python;
    }
    if (ext_in(ext, {"js", "mjs", "cjs", "ts", "tsx", "jsx", "vue", "svelte"})) {
        return Language::JavaScript;
    }
    if (ext_in(ext, {"rs"})) {
        return Language::Rust;
    }
    if (ext_in(ext, {"go"})) {
        return Language::Go;
    }
    if (ext_in(ext, {"sh", "bash", "zsh", "fish", "ps1"})) {
        return Language::Shell;
    }
    if (ext_in(ext, {"md", "markdown"})) {
        return Language::Markdown;
    }
    if (ext_in(ext, {"json", "jsonc"})) {
        return Language::Json;
    }
    if (ext_in(ext, {"yaml", "yml", "toml"})) {
        return Language::Yaml;
    }
    if (ext_in(ext, {"html", "htm", "xml", "svg", "xhtml"})) {
        return Language::Html;
    }
    if (ext_in(ext, {"css", "scss", "sass", "less"})) {
        return Language::Css;
    }
    if (ext_in(ext, {"sql", "psql"})) {
        return Language::Sql;
    }
    return Language::Generic;
}

Language language_from_path(std::string_view path) noexcept {
    const std::string ext = extension_from_path(path);
    if (!ext.empty()) {
        return language_from_extension(ext);
    }

    const std::string lower = to_lower(std::string(path));
    if (ends_with(lower, "dockerfile") || lower.find("makefile") != std::string::npos) {
        return Language::Shell;
    }
    if (ends_with(lower, "cmakelists.txt")) {
        return Language::Generic;
    }
    return Language::Generic;
}

LoadedFile load_file(const std::string& path) {
    LoadedFile result;
    result.path = path;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = "cannot open file: " + path;
        return result;
    }

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    result.lines = split_lines(content);
    result.language = language_from_path(path);
    result.ok = true;
    return result;
}

bool save_file(const std::string& path, const std::vector<std::string>& lines, std::string& error) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        error = "cannot write file: " + path;
        return false;
    }

    for (std::size_t i = 0; i < lines.size(); ++i) {
        out << lines[i];
        if (i + 1 < lines.size()) {
            out << '\n';
        }
    }
    return true;
}

}  // namespace askdocs
