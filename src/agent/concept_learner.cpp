#include "askdocs/agent/concept_learner.hpp"

#include "askdocs/docs/online_search.hpp"
#include "askdocs/nlp/tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace askdocs::agent {

namespace {

bool is_stopword(std::string_view tok) {
    static const char* kStop[] = {
        "teach", "me",   "the",  "concept", "used", "in",   "line", "a",    "an",   "what",
        "does",  "do",   "is",   "how",     "why",  "at",   "on",   "of",   "to",   "for",
        "and",   "or",   "are",  "about",   "tell", "work", "mean", "define", "your", "this",
    };
    for (const char* w : kStop) {
        if (tok == w) {
            return true;
        }
    }
    return false;
}

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string capitalize(std::string s) {
    if (s.empty()) {
        return s;
    }
    s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

std::string join_symbols(const std::vector<std::string>& symbols, std::size_t max_count) {
    std::ostringstream oss;
    const std::size_t n = std::min(symbols.size(), max_count);
    for (std::size_t i = 0; i < n; ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << symbols[i];
    }
    if (symbols.size() > max_count) {
        oss << ", ...";
    }
    return oss.str();
}

void merge_hit(std::vector<docs::OnlineDocHit>& hits, const docs::OnlineDocHit& incoming) {
    if (incoming.url.empty()) {
        return;
    }
    for (const docs::OnlineDocHit& hit : hits) {
        if (hit.url == incoming.url) {
            return;
        }
    }
    hits.push_back(incoming);
}

std::string to_lower_copy(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string strip_question_suffix(std::string text) {
    while (!text.empty() && (text.back() == '?' || text.back() == '!' ||
                             std::isspace(static_cast<unsigned char>(text.back())))) {
        text.pop_back();
    }
    return trim(text);
}

}  // namespace

bool is_open_topic_query(std::string_view query) {
    if (query_targets_specific_line(query)) {
        return false;
    }

    const std::string lower = to_lower_copy(query);
    if (lower.find("error") != std::string::npos || lower.find("bug") != std::string::npos ||
        lower.find("crash") != std::string::npos || lower.find("fix ") != std::string::npos) {
        return false;
    }
    if (lower.find("where is") != std::string::npos ||
        (lower.find("find ") != std::string::npos && lower.find("file") != std::string::npos)) {
        return false;
    }

    static const std::regex kTopicPatterns[] = {
        std::regex(R"(\bwhat\s+(?:is|are)\b)", std::regex::icase),
        std::regex(R"(\bexplain\b)", std::regex::icase),
        std::regex(R"(\btell\s+me\s+about\b)", std::regex::icase),
        std::regex(R"(\bhow\s+(?:do|does)\b)", std::regex::icase),
        std::regex(R"(\bdefine\b)", std::regex::icase),
        std::regex(R"(\bwhat\s+does\b.+\bmean\b)", std::regex::icase),
    };

    for (const std::regex& re : kTopicPatterns) {
        if (std::regex_search(lower, re)) {
            return true;
        }
    }

    if (query.find('@') != std::string_view::npos) {
        return false;
    }

    const std::string topic = extract_topic_from_query(query);
    if (topic.empty()) {
        return false;
    }

    int content_tokens = 0;
    for (const std::string& tok : nlp::tokenize(query)) {
        if (!is_stopword(tok) && tok.size() >= 2) {
            ++content_tokens;
        }
    }
    return content_tokens >= 1 && content_tokens <= 4;
}

std::string extract_topic_from_query(std::string_view query) {
    std::string text = strip_question_suffix(std::string(query));
    text = to_lower_copy(text);

    static const std::regex kStripPrefixes[] = {
        std::regex(R"(^what\s+(?:is|are)\s+(?:a|an|the\s+)?)", std::regex::icase),
        std::regex(R"(^explain\s+(?:what\s+)?(?:a|an|the\s+)?)", std::regex::icase),
        std::regex(R"(^tell\s+me\s+about\s+(?:a|an|the\s+)?)", std::regex::icase),
        std::regex(R"(^how\s+(?:do|does)\s+(?:a|an|the\s+)?)", std::regex::icase),
        std::regex(R"(^define\s+(?:a|an|the\s+)?)", std::regex::icase),
        std::regex(R"(^what\s+does\s+(?:a|an|the\s+)?)", std::regex::icase),
        std::regex(R"(^can\s+you\s+explain\s+(?:a|an|the\s+)?)", std::regex::icase),
    };

    for (const std::regex& re : kStripPrefixes) {
        text = std::regex_replace(text, re, "");
    }

    text = std::regex_replace(text, std::regex(R"(\s+work\s*$)", std::regex::icase), "");
    text = std::regex_replace(text, std::regex(R"(\s+mean\s*$)", std::regex::icase), "");
    text = strip_question_suffix(trim(text));

    std::ostringstream topic;
    for (const std::string& tok : nlp::tokenize(text)) {
        if (is_stopword(tok) || tok.size() < 2) {
            continue;
        }
        if (!topic.str().empty()) {
            topic << ' ';
        }
        topic << tok;
    }

    return trim(topic.str());
}

namespace {

std::string infer_concept_name(const LineSnapshot* line, const docs::OnlineDocHit* online,
                               std::string_view user_query) {
    if (online != nullptr && !online->title.empty()) {
        return online->title;
    }
    if (line == nullptr || !line->valid) {
        const std::string topic = extract_topic_from_query(user_query);
        if (!topic.empty()) {
            return topic;
        }
        return "programming concept";
    }

    std::string lower = line->line_text;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (line->line_text.find("+=") != std::string::npos ||
        line->line_text.find("-=") != std::string::npos ||
        line->line_text.find("*=") != std::string::npos) {
        return "compound assignment";
    }
    if (lower.find("for ") != std::string::npos) {
        return "for loop";
    }
    if (lower.find("while ") != std::string::npos) {
        return "while loop";
    }
    if (lower.find("if ") != std::string::npos) {
        return "conditional";
    }
    if (lower.find("return") != std::string::npos) {
        return "return";
    }
    if (line->line_text.find("->") != std::string::npos) {
        return "pointer member access";
    }
    if (lower.find("class ") != std::string::npos || lower.find("struct ") != std::string::npos) {
        return "type definition";
    }
    if (lower.find("def ") != std::string::npos) {
        return "function definition";
    }
    if (line->line_text.find('=') != std::string::npos && line->line_text.find("==") == std::string::npos) {
        return "assignment";
    }

    if (!line->symbols.empty()) {
        return line->symbols.front();
    }
    return "statement";
}

std::string explain_concept_on_line(const LineSnapshot& snap, std::string_view concept_name) {
    std::ostringstream out;
    const std::string lower_concept = [&]() {
        std::string s(concept_name);
        for (char& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    }();

    if (lower_concept.find("compound assignment") != std::string::npos ||
        snap.line_text.find("+=") != std::string::npos) {
        out << "This updates a variable in place: read the current value, apply the operation, "
               "and write it back without a separate temporary.";
    } else if (lower_concept.find("for loop") != std::string::npos ||
               lower_concept.find("while loop") != std::string::npos) {
        out << "This repeats a block while a condition or iterator allows — each iteration may "
               "change program state before the next check.";
    } else if (lower_concept.find("conditional") != std::string::npos) {
        out << "This chooses one branch based on a boolean condition — only the matching path runs.";
    } else if (lower_concept.find("pointer") != std::string::npos ||
               snap.line_text.find("->") != std::string::npos) {
        out << "This follows a pointer to reach a member or value stored elsewhere in memory.";
    } else if (lower_concept.find("return") != std::string::npos) {
        out << "This hands control back to the caller, optionally passing a value out.";
    } else if (lower_concept.find("assignment") != std::string::npos) {
        out << "This binds or updates a name with a value — the right-hand side is evaluated first.";
    } else {
        out << "This line applies \"" << concept_name
            << "\" — trace which names change and what values flow through them.";
    }
    return out.str();
}

std::string build_search_query(const LineSnapshot* line, std::string_view user_query) {
    if (line == nullptr || !line->valid) {
        const std::string topic = extract_topic_from_query(user_query);
        if (!topic.empty()) {
            return topic;
        }
    }

    std::ostringstream combined;
    for (const std::string& tok : nlp::tokenize(user_query)) {
        if (!is_stopword(tok) && tok.size() >= 2) {
            combined << tok << ' ';
        }
    }
    if (line != nullptr && line->valid) {
        combined << line->line_text << ' ';
        for (const std::string& sym : line->symbols) {
            combined << sym << ' ';
        }
    }
    return trim(combined.str());
}

}  // namespace

ConceptSearchResult search_concepts(const LineSnapshot* line, std::string_view user_query,
                                    std::string_view lang) {
    ConceptSearchResult result;
    result.query_used = build_search_query(line, user_query);

    result.online_hits = docs::search_online_docs(result.query_used, lang, 5);

    if (line != nullptr && line->valid) {
        for (const docs::OnlineDocHit& sym_hit :
             docs::search_online_docs(join_symbols(line->symbols, 6), lang, 2)) {
            merge_hit(result.online_hits, sym_hit);
        }
    }

    if (result.online_hits.size() > 5) {
        result.online_hits.resize(5);
    }

    if (!result.online_hits.empty()) {
        result.primary = result.online_hits.front();
    }

    result.concept_name =
        infer_concept_name(line, result.online_hits.empty() ? nullptr : &result.primary, user_query);
    return result;
}

std::string explain_topic_overview(std::string_view concept_name,
                                   const std::vector<docs::OnlineDocHit>& online_hits) {
    for (const docs::OnlineDocHit& hit : online_hits) {
        if (!hit.snippet.empty()) {
            return hit.snippet;
        }
    }

    std::string lower(concept_name);
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::ostringstream out;
    if (lower.find("null pointer") != std::string::npos || lower.find("nullptr") != std::string::npos ||
        lower.find("null reference") != std::string::npos) {
        out << "A null pointer (or null reference) is a special value meaning the pointer does not "
               "refer to any object in memory.\n"
            << "Dereferencing a null pointer is undefined behavior in C/C++ and typically causes a "
               "crash or segmentation fault. Always check for nullptr before use.";
    } else if (lower.find("array") != std::string::npos) {
        out << "An array stores multiple values of the same type in contiguous memory.\n"
            << "You access elements by index (0-based in C/C++/Python). "
            << "Fixed-size arrays live on the stack or static storage in C/C++; "
            << "Python lists and C++ std::vector grow dynamically.";
    } else if (lower.find("vector") != std::string::npos) {
        out << "A vector is a resizable sequence — like an array that can grow or shrink.\n"
            << "In C++, std::vector manages heap memory for you; in Python, list is the "
            << "built-in dynamic array.";
    } else if (lower.find("pointer") != std::string::npos) {
        out << "A pointer holds the address of another value in memory.\n"
            << "You dereference it (*ptr in C/C++) to read or write the target. "
            << "Pointers enable dynamic data structures and efficient parameter passing.";
    } else if (lower.find("loop") != std::string::npos || lower.find("for") != std::string::npos ||
               lower.find("while") != std::string::npos) {
        out << "A loop repeats a block of code while a condition holds or over a sequence.\n"
            << "Each iteration can update state; the loop exits when the condition fails or "
            << "the sequence is exhausted.";
    } else if (lower.find("function") != std::string::npos || lower.find("method") != std::string::npos) {
        out << "A function packages reusable logic with named parameters and an optional return value.\n"
            << "Callers pass arguments in; the function runs its body and may produce a result "
            << "or side effects.";
    } else if (lower.find("class") != std::string::npos || lower.find("struct") != std::string::npos ||
               lower.find("object") != std::string::npos) {
        out << "A class (or struct) groups data and behavior into one type.\n"
            << "Instances carry their own state; methods define what they can do.";
    } else if (lower.find("string") != std::string::npos) {
        out << "A string is an ordered sequence of characters for text.\n"
            << "Languages provide types (std::string, Python str) with operations like "
            << "concatenation, slicing, and searching.";
    } else if (lower.find("variable") != std::string::npos) {
        out << "A variable is a named location in memory that holds a value.\n"
            << "You declare its type (in C/C++) or bind a name to a value (in Python), "
            << "then read or update it as the program runs.";
    } else if (lower.find("recursion") != std::string::npos) {
        out << "Recursion is when a function calls itself with a smaller or simpler input.\n"
            << "Every recursive function needs a base case to stop and a step that moves "
            << "toward that base case.";
    } else {
        std::string title(concept_name);
        if (!title.empty()) {
            title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
        }
        out << title << " is a core programming idea — see the references below for definitions, "
               "examples, and common pitfalls.";
    }

    return out.str();
}

std::string compose_learn_response(const LineSnapshot* line, const ConceptSearchResult& search,
                                   const ResolvedContext& ctx) {
    (void)ctx;
    std::ostringstream out;
    out << capitalize(search.concept_name) << "\n\n";

    if (line != nullptr && line->valid) {
        out << "Line " << line->ref.line_number;
        if (!line->filepath.empty()) {
            out << " in " << line->filepath;
        }
        out << ":\n";
        for (const auto& row : line->context) {
            out << (row.first == line->ref.line_number ? "> " : "  ") << row.first << " | "
                << row.second << '\n';
        }
        out << "\nWhat this concept does here:\n";
        out << explain_concept_on_line(*line, search.concept_name) << "\n";
    } else {
        out << explain_topic_overview(search.concept_name, search.online_hits) << "\n";
    }

    if (!search.online_hits.empty()) {
        out << "\nReferences:\n";
        for (std::size_t i = 0; i < search.online_hits.size() && i < 3; ++i) {
            const docs::OnlineDocHit& hit = search.online_hits[i];
            out << "· " << hit.title << "\n  " << hit.url << '\n';
        }
    }

    return out.str();
}

}  // namespace askdocs::agent
