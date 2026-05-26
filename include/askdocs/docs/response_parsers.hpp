#pragma once

#include "askdocs/docs/online_search.hpp"

#include <string>
#include <vector>

namespace askdocs::docs {

// Parse Wikipedia OpenSearch JSON: ["query", ["titles"...], ["desc"...], ["urls"...]]
std::vector<OnlineDocHit> parse_wikipedia_opensearch_json(const std::string& json);

// Parse Wikipedia REST summary JSON (extract field).
std::string parse_wikipedia_summary_extract(const std::string& json);

// Parse cppreference / python docs HTML search result pages.
std::vector<OnlineDocHit> parse_cppreference_html(const std::string& html);
std::vector<OnlineDocHit> parse_python_docs_html(const std::string& html);

// Extract definition text and code examples from a doc article page.
std::string parse_cppreference_article_snippet(const std::string& html);
std::string parse_python_docs_article_snippet(const std::string& html);

// DevDocs (devdocs.io) C++ index search + page snippets.
std::vector<OnlineDocHit> parse_devdocs_index_search(const std::string& index_json,
                                                     std::string_view query,
                                                     std::size_t limit = 5);
std::string parse_devdocs_page_snippet(const std::string& html);

}  // namespace askdocs::docs
