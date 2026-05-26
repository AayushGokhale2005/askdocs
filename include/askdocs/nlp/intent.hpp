#pragma once

#include <array>
#include <string_view>

namespace askdocs::nlp {

enum class UserIntent {
    Hint = 0,
    Explain,
    Debug,
    Quiz,
    LearningPath,
    NavigateCode,
    Unknown,
};

struct IntentResult {
    UserIntent intent = UserIntent::Unknown;
    float confidence = 0.0f;
};

// Minimal 2-layer feedforward net (BoW -> hidden -> softmax).
class IntentNet {
public:
    static constexpr std::size_t kInput = 48;
    static constexpr std::size_t kHidden = 12;
    static constexpr std::size_t kOutput = 7;

    IntentResult classify(const std::array<float, kInput>& input) const noexcept;

private:
    static float relu(float x) noexcept;
    static float softmax_sum(const std::array<float, kOutput>& logits) noexcept;
};

const char* intent_name(UserIntent intent) noexcept;

}  // namespace askdocs::nlp
