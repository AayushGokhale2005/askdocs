#include "askdocs/docs/response_parsers.hpp"
#include "askdocs/nlp/tokenizer.hpp"

#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace askdocs::docs {

namespace {

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

void add_unique(std::vector<OnlineDocHit>& hits, OnlineDocHit hit) {
    hit.title = trim(hit.title);
    hit.snippet = trim(hit.snippet);
    if (hit.title.empty() || hit.url.empty()) {
        return;
    }
    for (const OnlineDocHit& existing : hits) {
        if (existing.url == hit.url) {
            return;
        }
    }
    hits.push_back(std::move(hit));
}

std::string absolute_cppreference_url(const char* href) {
    if (href == nullptr || href[0] == '\0') {
        return {};
    }
    const std::string path(href);
    if (path.rfind("http", 0) == 0) {
        return path;
    }
    return "https://en.cppreference.com" + path;
}

std::string absolute_python_url(const char* href) {
    if (href == nullptr || href[0] == '\0') {
        return {};
    }
    const std::string path(href);
    if (path.rfind("http", 0) == 0) {
        return path;
    }
    if (path[0] != '/') {
        return "https://docs.python.org/3/" + path;
    }
    return "https://docs.python.org" + path;
}

std::string to_lower_copy(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool is_devdocs_stopword(std::string_view tok) {
    static const char* kStop[] = {
        "teach", "me",   "the",  "concept", "used", "in",   "line", "a",    "an",   "what",
        "does",  "do",   "is",   "how",     "why",  "at",   "on",   "of",   "to",   "for",
        "and",   "or",   "are",  "about",   "tell", "work", "mean", "define", "your", "this",
        "element", "elements",
    };
    for (const char* w : kStop) {
        if (tok == w) {
            return true;
        }
    }
    return false;
}

void expand_devdocs_query_tokens(std::vector<std::string>& tokens) {
    static const char* kSynonyms[][2] = {
        {"delete", "erase"}, {"remove", "erase"}, {"add", "push"},
        {"insert", "emplace"}, {"length", "size"}, {"initialize", "init"},
        {"initialise", "init"}, {"create", "construct"},
    };

    const std::vector<std::string> original = tokens;
    for (const auto& pair : kSynonyms) {
        for (const std::string& tok : original) {
            if (tok == pair[0]) {
                tokens.push_back(pair[1]);
            }
        }
    }
}

}  // namespace

std::vector<OnlineDocHit> parse_wikipedia_opensearch_json(const std::string& json) {
    std::vector<OnlineDocHit> hits;

    const nlohmann::json parsed = nlohmann::json::parse(json, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_array() || parsed.size() < 4) {
        return hits;
    }

    const nlohmann::json& titles = parsed[1];
    const nlohmann::json& descriptions = parsed[2];
    const nlohmann::json& urls = parsed[3];
    if (!titles.is_array() || !urls.is_array()) {
        return hits;
    }

    const std::size_t count = std::min(titles.size(), urls.size());
    for (std::size_t i = 0; i < count && hits.size() < 5; ++i) {
        if (!titles[i].is_string() || !urls[i].is_string()) {
            continue;
        }
        OnlineDocHit hit;
        hit.title = titles[i].get<std::string>();
        hit.url = urls[i].get<std::string>();
        hit.source = "web";
        hit.score = 0.6f;
        if (descriptions.is_array() && i < descriptions.size() && descriptions[i].is_string()) {
            hit.snippet = descriptions[i].get<std::string>();
        }
        add_unique(hits, std::move(hit));
    }

    return hits;
}

std::string parse_wikipedia_summary_extract(const std::string& json) {
    const nlohmann::json parsed = nlohmann::json::parse(json, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return {};
    }
    const auto it = parsed.find("extract");
    if (it == parsed.end() || !it->is_string()) {
        return {};
    }
    return trim(it->get<std::string>());
}

std::vector<OnlineDocHit> parse_cppreference_html(const std::string& html) {
    std::vector<OnlineDocHit> hits;

    pugi::xml_document doc;
    const pugi::xml_parse_result result =
        doc.load_string(html.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!result) {
        return hits;
    }

    for (pugi::xpath_node xpath :
         doc.select_nodes("//div[contains(@class,'mw-search-result-heading')]/a")) {
        const pugi::xml_node anchor = xpath.node();
        OnlineDocHit hit;
        hit.url = absolute_cppreference_url(anchor.attribute("href").as_string());
        hit.title = trim(anchor.text().as_string());
        hit.source = "cppreference.com";
        hit.score = 1.0f;
        add_unique(hits, std::move(hit));
        if (hits.size() >= 5) {
            break;
        }
    }

    if (!hits.empty()) {
        return hits;
    }

    for (pugi::xpath_node xpath : doc.select_nodes("//a[starts-with(@href,'/w/')]")) {
        const pugi::xml_node anchor = xpath.node();
        OnlineDocHit hit;
        hit.url = absolute_cppreference_url(anchor.attribute("href").as_string());
        hit.title = trim(anchor.text().as_string());
        hit.source = "cppreference.com";
        hit.score = 0.9f;
        add_unique(hits, std::move(hit));
        if (hits.size() >= 5) {
            break;
        }
    }

    return hits;
}

std::vector<OnlineDocHit> parse_python_docs_html(const std::string& html) {
    std::vector<OnlineDocHit> hits;

    pugi::xml_document doc;
    const pugi::xml_parse_result result =
        doc.load_string(html.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!result) {
        return hits;
    }

    for (pugi::xpath_node xpath :
         doc.select_nodes("//a[contains(@class,'match') or contains(@class,'search')]")) {
        const pugi::xml_node anchor = xpath.node();
        const std::string href = anchor.attribute("href").as_string();
        if (href.empty() || href.find("search.html") != std::string::npos) {
            continue;
        }
        OnlineDocHit hit;
        hit.url = absolute_python_url(href.c_str());
        hit.title = trim(anchor.text().as_string());
        hit.source = "docs.python.org";
        hit.score = 1.0f;
        add_unique(hits, std::move(hit));
        if (hits.size() >= 5) {
            break;
        }
    }

    if (!hits.empty()) {
        return hits;
    }

    for (pugi::xpath_node xpath : doc.select_nodes("//li/a[contains(@href,'.html')]")) {
        const pugi::xml_node anchor = xpath.node();
        OnlineDocHit hit;
        hit.url = absolute_python_url(anchor.attribute("href").as_string());
        hit.title = trim(anchor.text().as_string());
        hit.source = "docs.python.org";
        hit.score = 0.8f;
        add_unique(hits, std::move(hit));
        if (hits.size() >= 5) {
            break;
        }
    }

    return hits;
}

std::string parse_cppreference_article_snippet(const std::string& html) {
    pugi::xml_document doc;
    const pugi::xml_parse_result result =
        doc.load_string(html.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!result) {
        return {};
    }

    std::ostringstream out;
    for (pugi::xpath_node xpath :
         doc.select_nodes("//div[contains(@class,'mw-parser-output')]//p")) {
        const std::string text = trim(xpath.node().text().as_string());
        if (text.size() < 40 || text.find("cppreference") != std::string::npos) {
            continue;
        }
        out << text;
        break;
    }

    int example_count = 0;
    for (pugi::xpath_node xpath :
         doc.select_nodes("//div[contains(@class,'t-example')]//pre|//div[contains(@class,'"
                          "t-example-reason')]//pre")) {
        const std::string code = trim(xpath.node().text().as_string());
        if (code.empty() || code.size() > 400) {
            continue;
        }
        if (!out.str().empty()) {
            out << "\n\n";
        }
        out << code;
        ++example_count;
        if (example_count >= 3) {
            break;
        }
    }

    if (out.str().empty()) {
        for (pugi::xpath_node xpath : doc.select_nodes("//div[contains(@class,'mw-parser-output')]//pre")) {
            const std::string code = trim(xpath.node().text().as_string());
            if (code.empty() || code.size() > 400) {
                continue;
            }
            out << code;
            break;
        }
    }

    return trim(out.str());
}

std::string parse_python_docs_article_snippet(const std::string& html) {
    pugi::xml_document doc;
    const pugi::xml_parse_result result =
        doc.load_string(html.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!result) {
        return {};
    }

    std::ostringstream out;
    for (pugi::xpath_node xpath :
         doc.select_nodes("//div[contains(@class,'body')]//p|//section//p")) {
        const std::string text = trim(xpath.node().text().as_string());
        if (text.size() < 30) {
            continue;
        }
        out << text;
        break;
    }

    int example_count = 0;
    for (pugi::xpath_node xpath :
         doc.select_nodes("//div[contains(@class,'highlight')]//pre|//pre[contains(@class,'python')]")) {
        const std::string code = trim(xpath.node().text().as_string());
        if (code.empty() || code.size() > 400) {
            continue;
        }
        if (!out.str().empty()) {
            out << "\n\n";
        }
        out << code;
        ++example_count;
        if (example_count >= 3) {
            break;
        }
    }

    return trim(out.str());
}

std::vector<OnlineDocHit> parse_devdocs_index_search(const std::string& index_json,
                                                     std::string_view query,
                                                     std::size_t limit) {
    std::vector<OnlineDocHit> hits;

    const nlohmann::json parsed = nlohmann::json::parse(index_json, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return hits;
    }

    const nlohmann::json* entries = nullptr;
    if (parsed.contains("entries") && parsed["entries"].is_array()) {
        entries = &parsed["entries"];
    } else if (parsed.is_array()) {
        entries = &parsed;
    }
    if (entries == nullptr) {
        return hits;
    }

    std::vector<std::string> tokens;
    for (const std::string& tok : nlp::tokenize(query)) {
        if (!is_devdocs_stopword(tok) && tok.size() >= 2) {
            tokens.push_back(to_lower_copy(tok));
        }
    }
    expand_devdocs_query_tokens(tokens);
    if (tokens.empty()) {
        return hits;
    }

    struct ScoredHit {
        OnlineDocHit hit;
        int score = 0;
    };
    std::vector<ScoredHit> scored;

    for (const nlohmann::json& entry : *entries) {
        if (!entry.is_object() || !entry.contains("name") || !entry.contains("path")) {
            continue;
        }
        if (!entry["name"].is_string() || !entry["path"].is_string()) {
            continue;
        }

        const std::string name = entry["name"].get<std::string>();
        const std::string path = entry["path"].get<std::string>();
        const std::string name_lower = to_lower_copy(name);
        const std::string path_lower = to_lower_copy(path);

        int score = 0;
        int token_hits = 0;
        for (const std::string& tok : tokens) {
            if (name_lower.find(tok) != std::string::npos) {
                score += 5;
                ++token_hits;
            } else if (path_lower.find(tok) != std::string::npos) {
                score += 2;
                ++token_hits;
            }
        }
        if (token_hits == 0) {
            continue;
        }

        OnlineDocHit hit;
        hit.title = name;
        hit.url = "https://devdocs.io/cpp/" + path;
        hit.source = "devdocs.io";
        hit.score = static_cast<float>(score);
        scored.push_back({std::move(hit), score});
    }

    std::sort(scored.begin(), scored.end(),
              [](const ScoredHit& a, const ScoredHit& b) { return a.score > b.score; });

    for (const ScoredHit& item : scored) {
        add_unique(hits, item.hit);
        if (hits.size() >= limit) {
            break;
        }
    }

    return hits;
}

std::string parse_devdocs_page_snippet(const std::string& html) {
    pugi::xml_document doc;
    const pugi::xml_parse_result result =
        doc.load_string(html.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
    if (!result) {
        return {};
    }

    std::ostringstream out;
    int signature_count = 0;
    for (pugi::xpath_node xpath : doc.select_nodes("//pre[@data-language='cpp']")) {
        const std::string code = trim(xpath.node().text().as_string());
        if (code.empty() || code.size() > 300) {
            continue;
        }
        if (!out.str().empty()) {
            out << "\n\n";
        }
        out << code;
        ++signature_count;
        if (signature_count >= 2) {
            break;
        }
    }

    for (pugi::xpath_node xpath : doc.select_nodes("//p")) {
        const std::string text = trim(xpath.node().text().as_string());
        if (text.size() < 20) {
            continue;
        }
        if (!out.str().empty()) {
            out << "\n\n";
        }
        out << text;
        break;
    }

    int example_count = 0;
    for (pugi::xpath_node xpath :
         doc.select_nodes("//div[contains(@class,'t-example')]//pre|//pre[@data-language='cpp-example']")) {
        const std::string code = trim(xpath.node().text().as_string());
        if (code.empty() || code.size() > 500) {
            continue;
        }
        if (!out.str().empty()) {
            out << "\n\n";
        }
        out << code;
        ++example_count;
        if (example_count >= 2) {
            break;
        }
    }

    return trim(out.str());
}

}  // namespace askdocs::docs
