#include "askdocs/nlp/intent.hpp"
#include "askdocs/nlp/tokenizer.hpp"

#include <cctype>
#include <unordered_map>

namespace askdocs::nlp {

namespace {

constexpr const char* kVocab[] = {
    "error",    "bug",       "debug",     "fix",       "crash",     "fail",
    "explain",  "why",       "how",       "what",      "understand", "mean",
    "hint",     "help",      "stuck",     "step",      "next",      "guide",
    "quiz",     "test",      "check",     "question",  "array",     "arrays",
    "path",     "learn",     "roadmap",   "prerequisite", "start",  "begin",
    "file",     "find",      "where",     "open",      "include",   "import",
    "loop",     "for",       "while",     "function",  "class",     "pointer",
    "vector",   "variable",  "type",      "python",    "cpp",       "c++",
};

static_assert(sizeof(kVocab) / sizeof(kVocab[0]) == IntentNet::kInput, "vocab size mismatch");

int vocab_index(std::string_view token) {
    for (std::size_t i = 0; i < IntentNet::kInput; ++i) {
        if (token == kVocab[i]) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;

    auto flush = [&]() {
        if (!current.empty()) {
            for (char& c : current) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            tokens.push_back(current);
            current.clear();
        }
    };

    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '#') {
            current.push_back(c);
        } else {
            flush();
        }
    }
    flush();
    return tokens;
}

std::vector<float> bag_of_words(std::string_view text) {
    std::vector<float> vec(IntentNet::kInput, 0.0f);
    for (const std::string& tok : tokenize(text)) {
        const int idx = vocab_index(tok);
        if (idx >= 0) {
            vec[static_cast<std::size_t>(idx)] = 1.0f;
        }
    }
    return vec;
}

}  // namespace askdocs::nlp
