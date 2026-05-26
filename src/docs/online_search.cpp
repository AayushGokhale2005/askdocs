#include "askdocs/docs/online_search.hpp"
#include "askdocs/docs/response_parsers.hpp"
#include "askdocs/nlp/tokenizer.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

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

std::string shell_single_quote(std::string_view text) {
    std::string out = "'";
    for (char c : text) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

std::string url_encode(std::string_view text) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << static_cast<char>(c);
        } else if (c == ' ') {
            encoded << '+';
        } else {
            encoded << '%' << std::setw(2) << static_cast<int>(c);
        }
    }
    return encoded.str();
}

std::string http_get(std::string_view url, std::size_t max_bytes = 48'000) {
    const std::string cmd =
        "curl -sL --max-time 12 -A 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
        "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36' " +
        shell_single_quote(url) + " 2>/dev/null";
    std::array<char, 4096> buf{};
    std::string body;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        return {};
    }
    while (body.size() < max_bytes) {
        const std::size_t n = std::fread(buf.data(), 1, buf.size(), pipe);
        if (n == 0) {
            break;
        }
        body.append(buf.data(), n);
    }
    pclose(pipe);
    if (body.size() > max_bytes) {
        body.resize(max_bytes);
    }
    return body;
}

std::string& devdocs_cpp_index_cache() {
    static std::string cache;
    return cache;
}

std::string fetch_devdocs_cpp_index() {
    std::string& cache = devdocs_cpp_index_cache();
    if (!cache.empty()) {
        return cache;
    }
    cache = http_get("https://documents.devdocs.io/cpp/index.json", 2'000'000);
    return cache;
}

std::vector<OnlineDocHit> search_devdocs_cpp(std::string_view query) {
    const std::string index = fetch_devdocs_cpp_index();
    if (index.empty()) {
        return {};
    }
    return parse_devdocs_index_search(index, query, 5);
}

std::string pick_query_terms(std::string_view query) {
    std::ostringstream out;
    int count = 0;
    for (const std::string& tok : nlp::tokenize(query)) {
        if (tok.size() < 2) {
            continue;
        }
        if (tok == "teach" || tok == "learn" || tok == "concept" || tok == "line" ||
            tok == "what" || tok == "does" || tok == "the" || tok == "in" || tok == "used" ||
            tok == "are" || tok == "is" || tok == "an" || tok == "a" || tok == "about" ||
            tok == "tell" || tok == "me" || tok == "work" || tok == "mean" || tok == "define" ||
            tok == "how" || tok == "why" || tok == "your" || tok == "this") {
            continue;
        }
        if (count > 0) {
            out << ' ';
        }
        out << tok;
        ++count;
        if (count >= 6) {
            break;
        }
    }
    const std::string terms = trim(out.str());
    return terms.empty() ? std::string(query) : terms;
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

std::vector<OnlineDocHit> search_python_docs(std::string_view query) {
    const std::string terms = pick_query_terms(query);
    const std::string url = "https://docs.python.org/3/search.html?q=" + url_encode(terms);
    return parse_python_docs_html(http_get(url));
}

std::vector<OnlineDocHit> search_web(std::string_view query) {
    const std::string terms = pick_query_terms(query);
    if (terms.empty()) {
        return {};
    }
    const std::string url = "https://en.wikipedia.org/w/api.php?action=opensearch&search=" +
                            url_encode(terms) + "&limit=5&namespace=0&format=json";
    std::vector<OnlineDocHit> hits = parse_wikipedia_opensearch_json(http_get(url));

    if (!hits.empty() && hits.front().snippet.empty()) {
        const std::string_view wiki_prefix = "https://en.wikipedia.org/wiki/";
        if (hits.front().url.rfind(wiki_prefix, 0) == 0) {
            const std::string page = hits.front().url.substr(wiki_prefix.size());
            const std::string summary_url =
                "https://en.wikipedia.org/api/rest_v1/page/summary/" + page;
            hits.front().snippet = parse_wikipedia_summary_extract(http_get(summary_url));
        }
    }

    return hits;
}

}  // namespace

bool online_search_enabled() {
    if (const char* env = std::getenv("ASKDOCS_ONLINE")) {
        return env[0] != '0';
    }
    return true;
}

std::vector<OnlineDocHit> search_online_docs(std::string_view query, std::string_view lang,
                                             std::size_t limit) {
    if (!online_search_enabled() || trim(std::string(query)).empty()) {
        return {};
    }

    std::vector<OnlineDocHit> hits;
    if (lang == "cpp" || lang == "c++" || lang == "c") {
        hits = search_devdocs_cpp(query);
    } else if (lang == "python" || lang == "py") {
        hits = search_python_docs(query);
    } else {
        for (OnlineDocHit& h : search_devdocs_cpp(query)) {
            add_unique(hits, std::move(h));
        }
        for (OnlineDocHit& h : search_python_docs(query)) {
            add_unique(hits, std::move(h));
        }
    }

    if (hits.empty()) {
        for (OnlineDocHit& h : search_web(query)) {
            add_unique(hits, std::move(h));
        }
    }

    if (hits.size() > limit) {
        hits.resize(limit);
    }
    return hits;
}

std::string fetch_doc_snippet(const OnlineDocHit& hit) {
    if (hit.url.empty()) {
        return {};
    }

    const std::string html = http_get(hit.url);
    if (html.empty()) {
        return {};
    }

    if (hit.source == "devdocs.io" || hit.url.find("devdocs.io/cpp/") != std::string::npos) {
        const std::string_view prefix = "https://devdocs.io/cpp/";
        if (hit.url.rfind(prefix, 0) == 0) {
            const std::string path = hit.url.substr(prefix.size());
            const std::string page_url = "https://documents.devdocs.io/cpp/" + path + ".html";
            return parse_devdocs_page_snippet(http_get(page_url));
        }
    }
    if (hit.source == "cppreference.com" || hit.url.find("cppreference.com") != std::string::npos) {
        return parse_cppreference_article_snippet(html);
    }
    if (hit.source == "docs.python.org" || hit.url.find("docs.python.org") != std::string::npos) {
        return parse_python_docs_article_snippet(html);
    }
    if (hit.source == "web" && hit.url.find("wikipedia.org") != std::string::npos) {
        const std::string_view wiki_prefix = "https://en.wikipedia.org/wiki/";
        if (hit.url.rfind(wiki_prefix, 0) == 0) {
            const std::string page = hit.url.substr(wiki_prefix.size());
            const std::string summary_url =
                "https://en.wikipedia.org/api/rest_v1/page/summary/" + page;
            return parse_wikipedia_summary_extract(http_get(summary_url));
        }
    }

    return {};
}

}  // namespace askdocs::docs
