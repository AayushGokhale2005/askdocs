#include "askdocs/agent/context_agent.hpp"
#include "askdocs/file_io.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace askdocs::agent {

ContextAgent::ContextAgent(std::string workspace_root, ContextBuffer& buffer)
    : workspace_root_(std::move(workspace_root)), buffer_(buffer) {}

std::string ContextAgent::resolve_path(const std::string& mention) const {
    fs::path p(mention);
    if (p.is_absolute()) {
        return p.string();
    }
    return (fs::path(workspace_root_) / p).lexically_normal().string();
}

std::vector<std::string> ContextAgent::extract_path_mentions(std::string_view message) const {
    std::vector<std::string> paths;
    static const std::regex re(R"((@)?([A-Za-z0-9_./-]+\.(cpp|cc|cxx|h|hpp|py|js|ts|rs|go|md)))");
    std::string text(message);
    for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) {
        paths.push_back((*it)[2].str());
    }
    return paths;
}

std::vector<std::string> ContextAgent::search_workspace(std::string_view filename,
                                                        std::size_t limit) const {
    std::vector<std::string> matches;
    std::error_code ec;
    if (!fs::exists(workspace_root_, ec)) {
        return matches;
    }

    const fs::path target_name(filename);
    int depth_limit = 5;
    std::function<void(const fs::path&, int)> walk = [&](const fs::path& dir, int depth) {
        if (depth > depth_limit || matches.size() >= limit) {
            return;
        }
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec || matches.size() >= limit) {
                break;
            }
            const std::string name = entry.path().filename().string();
            if (!name.empty() && name[0] == '.') {
                continue;
            }
            if (entry.is_directory(ec)) {
                if (name == "build" || name == "node_modules" || name == ".git") {
                    continue;
                }
                walk(entry.path(), depth + 1);
            } else if (entry.path().filename() == target_name) {
                matches.push_back(entry.path().string());
            }
        }
    };

    walk(workspace_root_, 0);
    return matches;
}

std::vector<std::string> ContextAgent::find_linked_files(const std::string& path) const {
    std::vector<std::string> linked;
    const LoadedFile file = load_file(path);
    if (!file.ok) {
        return linked;
    }

    static const std::regex include_re(R"(#include\s*[<"]([^">]+)[">])");
    static const std::regex import_re(R"(^(?:from|import)\s+([a-zA-Z0-9_.]+))");

    for (const std::string& line : file.lines) {
        std::smatch m;
        if (std::regex_search(line, m, include_re)) {
            linked.push_back(m[1].str());
        } else if (std::regex_search(line, m, import_re)) {
            linked.push_back(m[1].str());
        }
        if (linked.size() >= 4) {
            break;
        }
    }
    return linked;
}

ResolvedContext ContextAgent::resolve(std::string_view user_message, const EditorState& editor,
                                      const FileIndex& index, std::size_t max_files) {
    ResolvedContext ctx;

    if (!editor.filepath.empty() && editor.filepath != "untitled") {
        ctx.editor_snippet = editor.filepath + " @ " + std::to_string(editor.cursor.row + 1) + ":" +
                             std::to_string(editor.cursor.col + 1);
    }

    std::vector<std::string> candidates;
    if (!editor.filepath.empty() && editor.filepath != "untitled") {
        candidates.push_back(resolve_path(editor.filepath));
    }

    for (const std::string& tagged : index.extract_mentions(user_message)) {
        candidates.push_back(tagged);
    }

    for (const std::string& mention : extract_path_mentions(user_message)) {
        if (auto resolved = index.resolve_mention(mention)) {
            candidates.push_back(*resolved);
        } else {
            candidates.push_back(resolve_path(mention));
            for (const std::string& found : search_workspace(mention, 1)) {
                candidates.push_back(found);
            }
        }
    }

    if (!editor.filepath.empty() && editor.filepath != "untitled") {
        for (const std::string& link : find_linked_files(resolve_path(editor.filepath))) {
            for (const std::string& found : search_workspace(link, 1)) {
                candidates.push_back(found);
            }
        }
    }

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    for (const std::string& path : candidates) {
        if (ctx.files.size() >= max_files) {
            break;
        }
        std::error_code ec;
        if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) {
            continue;
        }

        const int focus =
            (!editor.filepath.empty() && editor.filepath != "untitled" &&
             path == resolve_path(editor.filepath))
                ? editor.cursor.row
                : 0;
        if (auto slice = buffer_.get(path, focus)) {
            ctx.files.push_back(*slice);
            ctx.resolved_paths.push_back(path);
        }
    }

    return ctx;
}

}  // namespace askdocs::agent
