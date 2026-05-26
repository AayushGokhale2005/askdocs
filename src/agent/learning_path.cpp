#include "askdocs/agent/learning_path.hpp"

#include "askdocs/docs/online_search.hpp"

#include <sstream>

namespace askdocs::agent {

void LearningPath::mark_seen(const std::string& doc_id) { seen_.insert(doc_id); }

std::vector<PathStep> LearningPath::build_path(std::string_view query, std::string_view lang,
                                               std::size_t horizon) const {
    const auto hits = docs::search_online_docs(query, lang, horizon);
    if (hits.empty()) {
        return {};
    }

    std::vector<PathStep> steps;
    for (const docs::OnlineDocHit& hit : hits) {
        PathStep step;
        step.doc_id = hit.url;
        step.title = hit.title;
        step.link = hit.url;
        step.completed = seen_.count(hit.url) > 0;
        steps.push_back(std::move(step));
    }
    return steps;
}

std::string LearningPath::format_path(std::string_view query, std::string_view lang) const {
    const auto steps = build_path(query, lang, 5);
    if (steps.empty()) {
        if (!docs::online_search_enabled()) {
            return "Online doc search is disabled. Unset ASKDOCS_ONLINE=0 and check your network.";
        }
        return "No learning path found — try a more specific topic (e.g. /path for loops).";
    }

    std::ostringstream out;
    out << "learning path (from online docs)\n";
    for (const PathStep& step : steps) {
        out << (step.completed ? "[x] " : "[ ] ") << step.title;
        if (!step.link.empty()) {
            out << "\n    " << step.link;
        }
        out << '\n';
    }
    return out.str();
}

}  // namespace askdocs::agent
