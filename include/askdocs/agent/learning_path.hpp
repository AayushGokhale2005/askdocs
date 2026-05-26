#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace askdocs::agent {

struct PathStep {
    std::string doc_id;
    std::string title;
    std::string link;
    bool completed = false;
};

class LearningPath {
public:
    void mark_seen(const std::string& doc_id);

    std::vector<PathStep> build_path(std::string_view query, std::string_view lang,
                                     std::size_t horizon = 5) const;
    std::string format_path(std::string_view query, std::string_view lang) const;

private:
    std::unordered_set<std::string> seen_;
};

}  // namespace askdocs::agent
