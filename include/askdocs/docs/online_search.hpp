#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace askdocs::docs {

struct OnlineDocHit {
    std::string title;
    std::string url;
    std::string snippet;
    std::string source;  // e.g. cppreference, docs.python.org
    float score = 0.0f;
};

// Search language docs on the web, then fall back to general web search.
// Returns empty if network/curl unavailable or ASKDOCS_ONLINE=0.
std::vector<OnlineDocHit> search_online_docs(std::string_view query, std::string_view lang,
                                             std::size_t limit = 3);

bool online_search_enabled();

}  // namespace askdocs::docs
