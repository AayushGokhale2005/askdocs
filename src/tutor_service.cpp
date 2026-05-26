#include "askdocs/tutor_service.hpp"

#include "askdocs/agent/concept_learner.hpp"
#include "askdocs/agent/line_analyzer.hpp"
#include "askdocs/docs/online_search.hpp"
#include "askdocs/file_io.hpp"
#include "askdocs/nlp/intent.hpp"
#include "askdocs/nlp/tokenizer.hpp"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace askdocs {

namespace {

std::string lang_from_editor(const EditorState& editor) {
    switch (editor.language) {
        case Language::Python:
            return "python";
        case Language::CFamily:
            return "cpp";
        default:
            return {};
    }
}

AiMode mode_from_intent(nlp::UserIntent intent, AiMode fallback) {
    switch (intent) {
        case nlp::UserIntent::Explain:
            return AiMode::Explain;
        case nlp::UserIntent::Debug:
            return AiMode::Debug;
        case nlp::UserIntent::Quiz:
            return AiMode::Quiz;
        default:
            return fallback;
    }
}

nlp::UserIntent resolve_intent(const TutorRequest& request, float& confidence) {
    const std::string query =
        request.command.payload.empty() ? request.user_text : request.command.payload;

    if (request.command.command_name == "path") {
        confidence = 1.0f;
        return nlp::UserIntent::LearningPath;
    }
    if (request.command.command_name == "explain") {
        confidence = 1.0f;
        return nlp::UserIntent::Explain;
    }
    if (request.command.command_name == "debug" || request.command.command_name == "trace") {
        confidence = 1.0f;
        return nlp::UserIntent::Debug;
    }
    if (request.command.command_name == "quiz") {
        confidence = 1.0f;
        return nlp::UserIntent::Quiz;
    }
    if (request.command.command_name == "hint" || request.command.command_name == "step") {
        confidence = 1.0f;
        return nlp::UserIntent::Hint;
    }
    if (request.command.command_name == "learn") {
        confidence = 1.0f;
        return nlp::UserIntent::Explain;
    }

    if (agent::is_open_topic_query(query)) {
        confidence = 1.0f;
        return nlp::UserIntent::Explain;
    }

    if (agent::query_targets_specific_line(query)) {
        confidence = 1.0f;
        return nlp::UserIntent::Explain;
    }

    const nlp::IntentNet net;
    const auto bow = nlp::bag_of_words(query);
    std::array<float, nlp::IntentNet::kInput> features{};
    for (std::size_t i = 0; i < bow.size() && i < features.size(); ++i) {
        features[i] = bow[i];
    }
    const nlp::IntentResult result = net.classify(features);
    confidence = result.confidence;
    return result.intent;
}

std::string append_online_section(const std::vector<docs::OnlineDocHit>& hits) {
    if (hits.empty()) {
        return {};
    }
    std::ostringstream out;
    const bool from_web = hits.front().source == "web";
    out << (from_web ? "\n\nWeb results:\n" : "\n\nOnline docs:\n");
    for (std::size_t i = 0; i < hits.size() && i < 3; ++i) {
        out << "· " << hits[i].title << "\n  " << hits[i].url << '\n';
    }
    return out.str();
}

std::string search_step_label(const docs::OnlineDocHit& hit) {
    if (hit.source == "web") {
        return "search web → " + hit.title;
    }
    return "search online docs → " + hit.title + " (" + hit.source + ")";
}

std::string topic_query_for(const TutorRequest& request) {
    if (!request.command.payload.empty()) {
        return request.command.payload;
    }
    if (request.command.command_name == "explain") {
        return {};
    }
    return request.user_text;
}

std::string search_empty_step_label() {
    if (!docs::online_search_enabled()) {
        return "search web → (disabled)";
    }
    return "search web → (no results)";
}

bool routes_to_topic_explain(const TutorRequest& request, std::string_view query) {
    if (request.command.command_name == "explain") {
        return true;
    }
    if (request.command.is_command) {
        return false;
    }
    return agent::is_open_topic_query(query);
}

}  // namespace

TutorService::TutorService(std::string workspace_root)
    : buffer_(), agent_(std::move(workspace_root), buffer_), path_() {
    refresh_index();
}

void TutorService::refresh_index() { index_.scan(agent_.workspace_root()); }

std::string TutorService::compose_response(const TutorRequest& request, nlp::UserIntent intent,
                                           const agent::ResolvedContext& ctx,
                                           const std::vector<docs::OnlineDocHit>& online_hits) {
    const std::string query =
        request.command.payload.empty() ? request.user_text : request.command.payload;

    if (request.command.mode == AiMode::Override) {
        return "Override mode: show your attempt first, then we can discuss a full approach.";
    }

    if (intent == nlp::UserIntent::LearningPath) {
        const auto steps = path_.build_path(query, lang_from_editor(request.editor), 5);
        for (const agent::PathStep& step : steps) {
            path_.mark_seen(step.doc_id);
        }
        return path_.format_path(query, lang_from_editor(request.editor));
    }

    std::ostringstream out;

    if (intent == nlp::UserIntent::Explain && !online_hits.empty()) {
        const std::string topic = agent::extract_topic_from_query(query);
        if (!topic.empty()) {
            out << agent::explain_topic_overview(topic, online_hits) << "\n\n";
        }
        out << online_hits.front().title << "\n";
        out << online_hits.front().url;
        if (online_hits.size() > 1) {
            out << "\n\nsee also: " << online_hits[1].url;
        }
        return out.str();
    }

    if (intent == nlp::UserIntent::Explain) {
        const std::string topic = agent::extract_topic_from_query(query);
        if (!topic.empty()) {
            out << agent::explain_topic_overview(topic, online_hits);
            if (!online_hits.empty()) {
                out << "\n\n" << online_hits.front().url;
            }
            return out.str();
        }
    }

    if (intent == nlp::UserIntent::Debug) {
        out << "Trace it step by step:\n1) expected output\n2) actual output\n3) first diverging line";
        if (!online_hits.empty()) {
            out << "\n\nrelated doc: " << online_hits.front().title << "\n  "
                << online_hits.front().url;
        }
        if (!ctx.files.empty()) {
            out << "\n\ncontext: " << ctx.files.front().path;
        }
        return out.str();
    }

    if (intent == nlp::UserIntent::Quiz && !online_hits.empty()) {
        out << "Quiz on " << online_hits.front().title << ":\n";
        out << "Explain the core idea in your own words, then check: " << online_hits.front().url;
        return out.str();
    }

    if (intent == nlp::UserIntent::NavigateCode) {
        if (ctx.resolved_paths.empty()) {
            return "Use @filename to attach a file slice — e.g. @app.cpp";
        }
        out << "Loaded slices:\n";
        for (const std::string& p : ctx.resolved_paths) {
            out << "· " << p << '\n';
        }
        return out.str();
    }

    if (!online_hits.empty()) {
        out << "What would you try first?";
    } else {
        out << "What have you tried so far?";
    }

    if (!ctx.files.empty()) {
        out << "\n\n@" << fs::path(ctx.files.front().path).filename().string() << " loaded ["
            << ctx.files.front().start_line + 1 << "-" << ctx.files.front().end_line << "]";
    }

    out << append_online_section(online_hits);
    return out.str();
}

TutorJob TutorService::build_learn_job(const TutorRequest& request) {
    TutorJob job;
    job.mode = AiMode::Learn;
    job.intent = nlp::UserIntent::Explain;

    const std::string query =
        request.command.payload.empty() ? request.user_text : request.command.payload;
    const std::string lang = lang_from_editor(request.editor);

    agent_.set_workspace(request.workspace_root);
    if (index_.size() == 0 || agent_.workspace_root() != request.workspace_root) {
        index_.scan(request.workspace_root);
    }

    job.steps.push_back({"① classify intent → learn_concept"});

    const agent::ResolvedContext ctx =
        agent_.resolve(request.user_text, request.editor, index_, 3);

    const auto mentions = index_.extract_mentions(request.user_text);
    if (!mentions.empty()) {
        for (const std::string& p : mentions) {
            job.steps.push_back({"② resolve @" + fs::path(p).filename().string() + " → " + p});
        }
    } else {
        job.steps.push_back({"② resolve @mentions → (none)"});
    }

    agent::LineSnapshot snap;
    const agent::LineSnapshot* snap_ptr = nullptr;

    if (const std::optional<agent::LineReference> line_ref = agent::parse_line_reference(query)) {
        std::string target_path = request.editor.filepath;
        if (!mentions.empty()) {
            target_path = mentions.front();
        }

        job.steps.push_back({"③ fetch line " + std::to_string(line_ref->line_number) + " from " +
                             (target_path.empty() ? "editor"
                                                  : fs::path(target_path).filename().string())});

        snap = agent::fetch_line_snapshot(request.editor, buffer_, target_path,
                                          line_ref->line_number, 2);
        if (!snap.valid) {
            job.steps.push_back({"④ search docs → (line unreadable)"});
            job.steps.push_back({"⑤ compose concept lesson"});
            job.response =
                "I couldn't read line " + std::to_string(line_ref->line_number) +
                ". Open the file or tag it: /learn teach me the concept in line " +
                std::to_string(line_ref->line_number) + " in @file.cpp";
            job.phase = TutorJob::Phase::Steps;
            return job;
        }

        snap.symbols = agent::extract_symbols(snap.line_text, request.editor.language);
        snap_ptr = &snap;

        std::string sym_summary;
        for (std::size_t i = 0; i < snap.symbols.size() && i < 4; ++i) {
            if (i > 0) {
                sym_summary += ", ";
            }
            sym_summary += snap.symbols[i];
        }
        if (snap.symbols.size() > 4) {
            sym_summary += ", ...";
        }
        job.steps.push_back({"④ extract keywords → " + (sym_summary.empty() ? "(none)" : sym_summary)});
    } else {
        job.steps.push_back({"③ fetch line → (topic query, no line number)"});
        job.steps.push_back({"④ extract keywords → message + editor context"});
    }

    std::string doc_lang = lang;
    if (doc_lang.empty() && snap_ptr != nullptr && !snap.filepath.empty()) {
        const Language detected = language_from_path(snap.filepath);
        if (detected == Language::Python) {
            doc_lang = "python";
        } else if (detected == Language::CFamily) {
            doc_lang = "cpp";
        }
    }

    const agent::ConceptSearchResult concept_search =
        agent::search_concepts(snap_ptr, query, doc_lang);

    if (!concept_search.online_hits.empty()) {
        job.steps.push_back({"⑤ " + search_step_label(concept_search.primary)});
        job.online_hits = concept_search.online_hits;
        job.doc_links.push_back(concept_search.primary.url);
        path_.mark_seen(concept_search.primary.url);
    } else {
        job.steps.push_back({"⑤ " + search_empty_step_label()});
    }

    if (!ctx.files.empty()) {
        for (const agent::FileSlice& slice : ctx.files) {
            job.steps.push_back({"⑥ load context " + fs::path(slice.path).filename().string() +
                                 " [" + std::to_string(slice.start_line + 1) + "-" +
                                 std::to_string(slice.end_line) + "]"});
        }
    } else {
        job.steps.push_back({"⑥ load context → editor window"});
    }

    job.steps.push_back({"⑦ compose concept lesson"});
    job.response = agent::compose_learn_response(snap_ptr, concept_search, ctx);
    job.phase = TutorJob::Phase::Steps;
    job.step_index = 0;
    job.char_index = 0;
    return job;
}

TutorJob TutorService::build_topic_job(const TutorRequest& request) {
    TutorJob job;
    job.mode = AiMode::Explain;
    job.intent = nlp::UserIntent::Explain;

    const std::string query = topic_query_for(request);
    const std::string lang = lang_from_editor(request.editor);
    const bool explicit_explain = request.command.command_name == "explain";
    const std::string intent_label = explicit_explain ? "explain" : "open_topic";

    agent_.set_workspace(request.workspace_root);
    if (index_.size() == 0 || agent_.workspace_root() != request.workspace_root) {
        index_.scan(request.workspace_root);
    }

    job.steps.push_back({"① classify intent → " + intent_label});

    if (query.empty()) {
        job.steps.push_back({"② extract topic → (none — add a subject)"});
        job.steps.push_back({"③ compose usage hint"});
        job.response =
            "What should I explain?\n\n"
            "Examples:\n"
            "  /explain arrays\n"
            "  /explain pointers\n"
            "  what are vectors\n"
            "  tell me about recursion";
        job.phase = TutorJob::Phase::Steps;
        job.step_index = 0;
        job.char_index = 0;
        return job;
    }

    const std::string topic = agent::extract_topic_from_query(query);
    job.steps.push_back({"② extract topic → " + (topic.empty() ? query : topic)});

    const agent::ConceptSearchResult concept_search = agent::search_concepts(nullptr, query, lang);

    if (!concept_search.online_hits.empty()) {
        job.steps.push_back({"③ " + search_step_label(concept_search.primary)});
        job.online_hits = concept_search.online_hits;
        job.doc_links.push_back(concept_search.primary.url);
        path_.mark_seen(concept_search.primary.url);
    } else {
        job.steps.push_back({"③ " + search_empty_step_label()});
    }

    const agent::ResolvedContext ctx =
        agent_.resolve(request.user_text, request.editor, index_, 3);

    if (!ctx.files.empty()) {
        for (const agent::FileSlice& slice : ctx.files) {
            job.steps.push_back({"④ load context " + fs::path(slice.path).filename().string() +
                                 " [" + std::to_string(slice.start_line + 1) + "-" +
                                 std::to_string(slice.end_line) + "]"});
        }
    } else if (!ctx.editor_snippet.empty()) {
        job.steps.push_back({"④ load context → editor window"});
    } else {
        job.steps.push_back({"④ load context → (general question)"});
    }

    job.steps.push_back({"⑤ compose topic lesson"});
    job.response = agent::compose_learn_response(nullptr, concept_search, ctx);
    job.phase = TutorJob::Phase::Steps;
    job.step_index = 0;
    job.char_index = 0;
    return job;
}

TutorJob TutorService::build_job(const TutorRequest& request) {
    if (request.command.command_name == "learn") {
        return build_learn_job(request);
    }

    const std::string query = topic_query_for(request);

    if (routes_to_topic_explain(request, query.empty() ? request.user_text : query)) {
        return build_topic_job(request);
    }

    TutorJob job;
    const std::string lang = lang_from_editor(request.editor);

    agent_.set_workspace(request.workspace_root);
    if (index_.size() == 0 || agent_.workspace_root() != request.workspace_root) {
        index_.scan(request.workspace_root);
    }

    if (const std::optional<agent::LineReference> line_ref = agent::parse_line_reference(query)) {
        job.intent = nlp::UserIntent::Explain;
        job.mode = AiMode::Explain;

        std::string target_path = request.editor.filepath;
        const auto mentions = index_.extract_mentions(request.user_text);
        if (!mentions.empty()) {
            target_path = mentions.front();
        }

        job.steps.push_back({"① classify intent → explain_line (" +
                             std::to_string(line_ref->line_number) + ")"});
        job.steps.push_back({"② fetch line " + std::to_string(line_ref->line_number) + " from " +
                             (target_path.empty() ? "editor" : fs::path(target_path).filename().string())});

        agent::LineSnapshot snap = agent::fetch_line_snapshot(
            request.editor, buffer_, target_path, line_ref->line_number, 2);

        if (!snap.valid) {
            job.steps.push_back({"③ line out of range — open the file or use @file.cpp"});
            job.steps.push_back({"④ compose response"});
            job.response = "I couldn't read line " + std::to_string(line_ref->line_number) +
                           ". Open the file in the editor or tag it: @app.cpp what does line " +
                           std::to_string(line_ref->line_number) + " do?";
            return job;
        }

        snap.symbols = agent::extract_symbols(snap.line_text, request.editor.language);

        std::string sym_summary;
        for (std::size_t i = 0; i < snap.symbols.size() && i < 4; ++i) {
            if (i > 0) {
                sym_summary += ", ";
            }
            sym_summary += snap.symbols[i];
        }
        if (snap.symbols.size() > 4) {
            sym_summary += ", ...";
        }
        job.steps.push_back({"③ extract symbols → " + (sym_summary.empty() ? "(none)" : sym_summary)});

        std::string doc_lang = lang;
        if (doc_lang.empty() && !snap.filepath.empty()) {
            const Language detected = language_from_path(snap.filepath);
            if (detected == Language::Python) {
                doc_lang = "python";
            } else if (detected == Language::CFamily) {
                doc_lang = "cpp";
            }
        }

        const auto online_hits = agent::cross_reference_online(snap, doc_lang);
        job.online_hits = online_hits;
        if (!online_hits.empty()) {
            job.steps.push_back({"④ " + search_step_label(online_hits.front())});
            job.doc_links.push_back(online_hits.front().url);
            path_.mark_seen(online_hits.front().url);
        } else {
            job.steps.push_back({"④ " + search_empty_step_label()});
        }

        job.steps.push_back({"⑤ compose line explanation"});
        job.response = agent::explain_line(snap, online_hits);
        job.phase = TutorJob::Phase::Steps;
        job.step_index = 0;
        job.char_index = 0;
        return job;
    }

    float confidence = 0.0f;
    const nlp::UserIntent intent = resolve_intent(request, confidence);
    job.intent = intent;
    job.mode = mode_from_intent(intent, request.command.mode);

    job.steps.push_back({std::string("① classify intent → ") + nlp::intent_name(intent) + " (" +
                         std::to_string(static_cast<int>(confidence * 100)) + "%)"});

    job.online_hits = docs::search_online_docs(query, lang, 3);
    if (!job.online_hits.empty()) {
        job.steps.push_back({"② " + search_step_label(job.online_hits.front())});
        job.doc_links.push_back(job.online_hits.front().url);
        path_.mark_seen(job.online_hits.front().url);
    } else {
        job.steps.push_back({"② " + search_empty_step_label()});
    }

    const auto mentions = index_.extract_mentions(request.user_text);
    if (!mentions.empty()) {
        for (const std::string& p : mentions) {
            job.steps.push_back({"③ resolve @" + fs::path(p).filename().string() + " → " + p});
        }
    } else {
        job.steps.push_back({"③ resolve @mentions → (none)"});
    }

    const agent::ResolvedContext ctx = agent_.resolve(request.user_text, request.editor, index_, 3);
    if (!ctx.files.empty()) {
        for (const agent::FileSlice& slice : ctx.files) {
            job.steps.push_back({"④ load slice " + fs::path(slice.path).filename().string() + " [" +
                                 std::to_string(slice.start_line + 1) + "-" +
                                 std::to_string(slice.end_line) + "] (" +
                                 std::to_string(slice.lines.size()) + " lines)"});
        }
    } else {
        job.steps.push_back({"④ load context buffer → editor window only"});
    }

    job.steps.push_back({"⑤ compose tutor response"});

    job.response = compose_response(request, intent, ctx, job.online_hits);
    job.phase = TutorJob::Phase::Steps;
    job.step_index = 0;
    job.char_index = 0;
    return job;
}

TutorJob TutorService::begin(const TutorRequest& request) { return build_job(request); }

std::future<TutorJob> TutorService::begin_async(const TutorRequest& request) {
    // Heavy work (curl / online search) runs on a separate thread so the UI
    // stays alive and can show the spinner while waiting.
    return std::async(std::launch::async, [this, request]() { return build_job(request); });
}

bool TutorService::tick(TutorJob& job, std::string& step_out, std::string& chunk_out) {
    step_out.clear();
    chunk_out.clear();

    if (job.phase == TutorJob::Phase::Done) {
        return false;
    }

    if (job.phase == TutorJob::Phase::Steps) {
        if (job.step_index < job.steps.size()) {
            step_out = job.steps[job.step_index++].text;
            return true;
        }
        job.phase = TutorJob::Phase::Response;
    }

    if (job.phase == TutorJob::Phase::Response) {
        if (job.char_index >= job.response.size()) {
            job.phase = TutorJob::Phase::Done;
            return false;
        }
        const std::size_t chunk = 12;
        chunk_out = job.response.substr(job.char_index, chunk);
        job.char_index += chunk_out.size();
        if (job.char_index >= job.response.size()) {
            job.phase = TutorJob::Phase::Done;
        }
        return true;
    }

    return false;
}

}  // namespace askdocs
