#pragma once

#include "askdocs/agent/context_buffer.hpp"
#include "askdocs/agent/file_index.hpp"
#include "askdocs/types.hpp"

#include <string>
#include <vector>

namespace askdocs::agent {

struct ResolvedContext {
    std::vector<FileSlice> files;
    std::vector<std::string> resolved_paths;
    std::string editor_snippet;
};

// Agentic resolver: picks files from message, editor, and shallow workspace search.
class ContextAgent {
public:
    ContextAgent(std::string workspace_root, ContextBuffer& buffer);

    void set_workspace(std::string root) { workspace_root_ = std::move(root); }
    const std::string& workspace_root() const noexcept { return workspace_root_; }

    ResolvedContext resolve(std::string_view user_message, const EditorState& editor,
                            const FileIndex& index, std::size_t max_files = 3);

    std::vector<std::string> find_linked_files(const std::string& path) const;

private:
    std::string workspace_root_;
    ContextBuffer& buffer_;

    std::vector<std::string> extract_path_mentions(std::string_view message) const;
    std::vector<std::string> search_workspace(std::string_view filename,
                                              std::size_t limit) const;
    std::string resolve_path(const std::string& mention) const;
};

}  // namespace askdocs::agent
