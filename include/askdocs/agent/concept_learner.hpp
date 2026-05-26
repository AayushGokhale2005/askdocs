#pragma once

#include "askdocs/agent/context_agent.hpp"
#include "askdocs/agent/line_analyzer.hpp"
#include "askdocs/docs/online_search.hpp"

#include <string>
#include <vector>

namespace askdocs::agent {

struct ConceptSearchResult {
    std::string query_used;
    std::string concept_name;
    std::vector<docs::OnlineDocHit> online_hits;
    docs::OnlineDocHit primary;
};

// Open-ended questions without @file or line numbers (e.g. "what are arrays").
bool is_open_topic_query(std::string_view query);
std::string extract_topic_from_query(std::string_view query);

ConceptSearchResult search_concepts(const LineSnapshot* line, std::string_view user_query,
                                    std::string_view lang);

std::string explain_topic_overview(std::string_view concept_name,
                                   const std::vector<docs::OnlineDocHit>& online_hits);

std::string compose_learn_response(const LineSnapshot* line, const ConceptSearchResult& search,
                                   const ResolvedContext& ctx);

}  // namespace askdocs::agent
