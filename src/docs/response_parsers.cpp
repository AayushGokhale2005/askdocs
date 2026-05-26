#include "askdocs/docs/response_parsers.hpp"

#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
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

}  // namespace askdocs::docs
