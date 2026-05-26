#pragma once

#include "askdocs/types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace askdocs {

struct LoadedFile {
    std::vector<std::string> lines;
    std::string path;
    Language language = Language::Generic;
    bool ok = false;
    std::string error;
};

LoadedFile load_file(const std::string& path);
bool save_file(const std::string& path, const std::vector<std::string>& lines, std::string& error);

Language language_from_extension(std::string_view ext) noexcept;
Language language_from_path(std::string_view path) noexcept;
std::string extension_from_path(std::string_view path);

std::string get_working_directory();
std::string resolve_path(const std::string& base, const std::string& relative);

}  // namespace askdocs
