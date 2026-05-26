#include "askdocs/nlp/intent.hpp"

#include <algorithm>
#include <cmath>

namespace askdocs::nlp {

namespace {

float score_range(const std::array<float, IntentNet::kInput>& input, std::size_t start,
                  std::size_t end) {
    float sum = 0.0f;
    for (std::size_t i = start; i < end && i < input.size(); ++i) {
        sum += input[i];
    }
    return sum;
}

}  // namespace

float IntentNet::relu(float x) noexcept { return x > 0.0f ? x : 0.0f; }

IntentResult IntentNet::classify(const std::array<float, IntentNet::kInput>& input) const noexcept {
    // Input groups (vocab indices): debug 0-5, explain 6-11, hint 12-17, quiz 18-23,
    // path 24-29, navigate 30-35, concepts 36-47.
    const float h_debug = relu(score_range(input, 0, 6));
    const float h_explain = relu(score_range(input, 6, 12));
    const float h_hint = relu(score_range(input, 12, 18));
    const float h_quiz = relu(score_range(input, 18, 24));
    const float h_path = relu(score_range(input, 24, 30));
    const float h_nav = relu(score_range(input, 30, 36));
    const float h_topic = relu(score_range(input, 36, 48) * 0.3f);

    std::array<float, kOutput> logits{
        h_hint + h_topic * 0.2f,
        h_explain + h_topic * 0.3f,
        h_debug + h_topic * 0.1f,
        h_quiz + h_topic * 0.1f,
        h_path + h_topic * 0.2f,
        h_nav + h_topic * 0.1f,
        0.1f,
    };

    float max_logit = logits[0];
    for (float v : logits) {
        max_logit = std::max(max_logit, v);
    }

    std::array<float, kOutput> probs{};
    float denom = 0.0f;
    for (std::size_t o = 0; o < kOutput; ++o) {
        probs[o] = std::exp(logits[o] - max_logit);
        denom += probs[o];
    }
    if (denom <= 0.0f) {
        return {UserIntent::Unknown, 0.0f};
    }

    std::size_t best = 0;
    for (std::size_t o = 1; o < kOutput; ++o) {
        if (probs[o] > probs[best]) {
            best = o;
        }
    }

    return {static_cast<UserIntent>(best), probs[best] / denom};
}

const char* intent_name(UserIntent intent) noexcept {
    switch (intent) {
        case UserIntent::Hint:
            return "hint";
        case UserIntent::Explain:
            return "explain";
        case UserIntent::Debug:
            return "debug";
        case UserIntent::Quiz:
            return "quiz";
        case UserIntent::LearningPath:
            return "path";
        case UserIntent::NavigateCode:
            return "navigate";
        case UserIntent::Unknown:
            return "unknown";
    }
    return "unknown";
}

}  // namespace askdocs::nlp
