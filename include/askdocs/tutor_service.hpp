#pragma once

#include "askdocs/agent/context_agent.hpp"
#include "askdocs/agent/context_buffer.hpp"
#include "askdocs/agent/file_index.hpp"
#include "askdocs/agent/learning_path.hpp"
#include "askdocs/command.hpp"
#include "askdocs/docs/online_search.hpp"
#include "askdocs/nlp/intent.hpp"
#include "askdocs/types.hpp"

#include <future>
#include <string>
#include <vector>

namespace askdocs {

struct AgentStep {
    std::string text;
};

struct TutorRequest {
    std::string user_text;
    ParsedCommand command;
    EditorState editor;
    std::string workspace_root;
};

struct TutorJob {
    std::vector<AgentStep> steps;
    std::string response;
    AiMode mode = AiMode::Learn;
    nlp::UserIntent intent = nlp::UserIntent::Unknown;
    std::vector<std::string> doc_links;
    std::vector<docs::OnlineDocHit> online_hits;
    std::size_t step_index = 0;
    std::size_t char_index = 0;
    enum class Phase { Steps, Response, Done } phase = Phase::Steps;
};

class TutorService {
public:
    explicit TutorService(std::string workspace_root);

    bool ready() const noexcept { return docs::online_search_enabled(); }

    agent::FileIndex& file_index() noexcept { return index_; }
    void refresh_index();

    TutorJob begin(const TutorRequest& request);
    std::future<TutorJob> begin_async(const TutorRequest& request);
    bool tick(TutorJob& job, std::string& step_out, std::string& chunk_out);

private:
    agent::ContextBuffer buffer_;
    agent::ContextAgent agent_;
    agent::LearningPath path_;
    agent::FileIndex index_;

    TutorJob build_job(const TutorRequest& request);
    TutorJob build_learn_job(const TutorRequest& request);
    TutorJob build_topic_job(const TutorRequest& request);
    std::string compose_response(const TutorRequest& request, nlp::UserIntent intent,
                                 const agent::ResolvedContext& ctx,
                                 const std::vector<docs::OnlineDocHit>& online_hits);
};

}  // namespace askdocs
