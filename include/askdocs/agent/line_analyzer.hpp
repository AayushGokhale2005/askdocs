#pragma once

#include "askdocs/agent/context_buffer.hpp"
#include "askdocs/docs/online_search.hpp"
#include "askdocs/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace askdocs::agent {

struct LineReference {
    int line_number = 0;  // 1-based
};

struct LineSnapshot {
    bool valid = false;
    LineReference ref;
    std::string filepath;
    std::string line_text;
    std::vector<std::pair<int, std::string>> context;  // line number, text
    std::vector<std::string> symbols;
};

std::optional<LineReference> parse_line_reference(std::string_view query);

LineSnapshot fetch_line_snapshot(const EditorState& editor, ContextBuffer& buffer,
                                 std::string_view filepath, int line_number,
                                 int context_radius = 2);

std::vector<std::string> extract_symbols(std::string_view line, Language lang);

std::vector<docs::OnlineDocHit> cross_reference_online(const LineSnapshot& snap,
                                                       std::string_view lang);

std::string explain_line(const LineSnapshot& snap,
                         const std::vector<docs::OnlineDocHit>& online_hits);

bool query_targets_specific_line(std::string_view query);

}  // namespace askdocs::agent
