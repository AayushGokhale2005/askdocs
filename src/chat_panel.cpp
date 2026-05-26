#include "askdocs/chat_panel.hpp"
#include "askdocs/command.hpp"
#include "askdocs/tutor_service.hpp"
#include "askdocs/utf8.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace askdocs {

namespace {

constexpr const char* kReset = "\x1b[0m";
constexpr const char* kDim = "\x1b[90m";
constexpr const char* kMuted = "\x1b[38;5;245m";
constexpr const char* kAgent = "\x1b[38;5;109m";
constexpr const char* kFocus = "\x1b[48;5;236m";
constexpr const char* kPick = "\x1b[38;5;117m";

// Agent pipeline palette
constexpr const char* kTree = "\x1b[38;5;238m";
constexpr const char* kStepNum = "\x1b[38;5;117m";
constexpr const char* kStepAction = "\x1b[38;5;252m";
constexpr const char* kStepSep = "\x1b[38;5;239m";
constexpr const char* kStepResult = "\x1b[38;5;114m";
constexpr const char* kStepWarn = "\x1b[38;5;214m";
constexpr const char* kStepMuted = "\x1b[38;5;243m";
constexpr const char* kUserAccent = "\x1b[38;5;255m";
constexpr const char* kUserPrompt = "\x1b[38;5;117m";
constexpr const char* kCmdSlash = "\x1b[38;5;109m";
constexpr const char* kMention = "\x1b[38;5;117m";
constexpr const char* kTutorText = "\x1b[38;5;252m";
constexpr const char* kHeaderMode = "\x1b[38;5;109m";
constexpr const char* kHeaderLive = "\x1b[38;5;214m";

enum class AgentStepLayout { Single, First, Middle, Last, Continuation };

const char* mode_label(AiMode mode) noexcept {
    switch (mode) {
        case AiMode::Learn:
            return "learn";
        case AiMode::Debug:
            return "debug";
        case AiMode::Explain:
            return "explain";
        case AiMode::Quiz:
            return "quiz";
        case AiMode::Override:
            return "override";
    }
    return "learn";
}

const char* role_prefix(MessageRole role) noexcept {
    switch (role) {
        case MessageRole::User:
            return "› ";
        case MessageRole::Tutor:
            return "  ";
        case MessageRole::System:
            return "· ";
        case MessageRole::Agent:
            return "  ";
    }
    return "  ";
}

bool is_spinner_text(std::string_view text) noexcept {
    return text.find("searching") != std::string_view::npos;
}

std::string_view strip_circled_number(std::string_view text, int& step_num) noexcept {
    static const char* kCircled[] = {"①", "②", "③", "④", "⑤", "⑥", "⑦", "⑧", "⑨", "⑩"};
    for (int i = 0; i < 10; ++i) {
        if (text.size() >= 3 && text.substr(0, 3) == kCircled[i]) {
            step_num = i + 1;
            text.remove_prefix(3);
            while (!text.empty() && text.front() == ' ') {
                text.remove_prefix(1);
            }
            return text;
        }
    }
    step_num = -1;
    return text;
}

std::pair<std::string_view, std::string_view> split_action_result(std::string_view body) noexcept {
    for (const char* sep : {" → ", " -> ", " — "}) {
        const std::size_t pos = body.find(sep);
        if (pos != std::string_view::npos) {
            return {body.substr(0, pos), body.substr(pos + std::char_traits<char>::length(sep))};
        }
    }
    return {body, {}};
}

std::string style_step_result(std::string_view result) {
    std::string out;
    if (result.find("offline") != std::string_view::npos ||
        result.find("disabled") != std::string_view::npos) {
        out.append(kStepWarn);
    } else if (result == "(none)" || result.find("(none)") != std::string_view::npos) {
        out.append(kStepMuted);
    } else {
        out.append(kStepResult);
    }
    out.append(result);
    return out;
}

std::string format_agent_step(std::string_view text, AgentStepLayout layout) {
    std::string out;
    out.append(kTree);
    switch (layout) {
        case AgentStepLayout::First:
            out.append("┌─");
            break;
        case AgentStepLayout::Middle:
            out.append("├─");
            break;
        case AgentStepLayout::Last:
            out.append("└─");
            break;
        case AgentStepLayout::Single:
            out.append("◆ ");
            break;
        case AgentStepLayout::Continuation:
            out.append("│ ");
            break;
    }
    out.append(kReset);

    if (is_spinner_text(text)) {
        out.append(kAgent);
        out.append(text);
        out.append(kReset);
        return out;
    }

    int step_num = -1;
    const std::string_view body = strip_circled_number(text, step_num);
    const auto [action, result] = split_action_result(body);

    if (step_num > 0) {
        out.append(kStepNum);
        out.push_back(static_cast<char>('0' + step_num));
        out.append(kReset);
        out.append(kStepSep);
        out.append(" ");
        out.append(kReset);
    }

    out.append(kStepAction);
    out.append(action);
    out.append(kReset);

    if (!result.empty()) {
        out.append(kStepSep);
        out.append(" · ");
        out.append(kReset);
        out.append(style_step_result(result));
        out.append(kReset);
    }

    return out;
}

bool token_starts_here(std::string_view text, std::size_t index) noexcept {
    return index == 0 || std::isspace(static_cast<unsigned char>(text[index - 1]));
}

std::string style_chat_tokens(std::string_view text, const char* plain_color) {
    std::string out;
    out.reserve(text.size() + 32);
    std::size_t i = 0;

    while (i < text.size()) {
        if (text[i] == '/' && token_starts_here(text, i)) {
            std::size_t j = i + 1;
            while (j < text.size() && !std::isspace(static_cast<unsigned char>(text[j]))) {
                ++j;
            }
            if (j > i + 1) {
                out.append(kCmdSlash);
                out.append(text.substr(i, j - i));
                out.append(kReset);
                out.append(plain_color);
                i = j;
                continue;
            }
        }
        if (text[i] == '@' && token_starts_here(text, i)) {
            std::size_t j = i + 1;
            while (j < text.size() && !std::isspace(static_cast<unsigned char>(text[j]))) {
                ++j;
            }
            if (j > i + 1) {
                out.append(kMention);
                out.append(text.substr(i, j - i));
                out.append(kReset);
                out.append(plain_color);
                i = j;
                continue;
            }
        }

        const std::size_t char_idx = utf8_char_count(text.substr(0, i));
        const std::string_view glyph = utf8_slice_by_chars(text, char_idx, 1);
        out.append(plain_color);
        out.append(glyph);
        i += glyph.size();
    }

    return out;
}

std::string styled_line(MessageRole role, std::string_view text) {
    std::string out;
    switch (role) {
        case MessageRole::User:
            out.append(kUserPrompt);
            out.append(role_prefix(role));
            out.append(kReset);
            out.append(style_chat_tokens(text, kUserAccent));
            break;
        case MessageRole::Tutor:
            out.append(kDim);
            out.append(role_prefix(role));
            out.append(kReset);
            out.append(style_urls_in_text(text, kTutorText));
            break;
        case MessageRole::System:
            out.append(kDim);
            out.append(role_prefix(role));
            out.append(kReset);
            out.append(style_chat_tokens(text, kMuted));
            break;
        case MessageRole::Agent:
            out.append(format_agent_step(text, AgentStepLayout::Single));
            break;
    }
    out.append(kReset);
    return out;
}

}  // namespace

ChatPanel::ChatPanel() {
    add_system("type @file.cpp · /learn /explain /hint /path /quiz /debug");
}

void ChatPanel::mark_all_dirty() noexcept {
    panel_dirty_ = true;
    content_dirty_ = true;
    header_dirty_ = true;
    dirty_from_cache_line_ = 0;
}

void ChatPanel::mark_content_dirty(int from_line) noexcept {
    content_dirty_ = true;
    dirty_from_cache_line_ = std::min(dirty_from_cache_line_, std::max(0, from_line));
}

void ChatPanel::scroll_up(int lines) noexcept {
    scroll_offset_ = std::max(0, scroll_offset_ - lines);
    mark_content_dirty(0);
}

void ChatPanel::scroll_down(int lines) noexcept {
    scroll_offset_ += lines;
    mark_content_dirty(0);
}

void ChatPanel::scroll_to_bottom() noexcept {
    scroll_offset_ = 1'000'000;
}

void ChatPanel::add_system(std::string text) {
    messages_.push_back({MessageRole::System, std::move(text), AiMode::Learn});
    mark_content_dirty(static_cast<int>(render_cache_.size()));
}

std::vector<std::string> ChatPanel::build_render_lines(int width) const {
    constexpr int kRolePrefixCols = 2;
    const int inner = std::max(1, width - kRolePrefixCols);
    std::vector<std::string> lines;

    for (std::size_t i = 0; i < messages_.size(); ++i) {
        const ChatMessage& msg = messages_[i];
        const bool prev_agent = i > 0 && messages_[i - 1].role == MessageRole::Agent;
        const bool next_agent =
            i + 1 < messages_.size() && messages_[i + 1].role == MessageRole::Agent;

        if (msg.role == MessageRole::Agent) {
            AgentStepLayout group = AgentStepLayout::Single;
            if (prev_agent && next_agent) {
                group = AgentStepLayout::Middle;
            } else if (prev_agent) {
                group = AgentStepLayout::Last;
            } else if (next_agent) {
                group = AgentStepLayout::First;
            }

            const std::vector<std::string> wrapped =
                wrap_text(msg.text, static_cast<std::size_t>(inner));
            for (std::size_t w = 0; w < wrapped.size(); ++w) {
                const AgentStepLayout layout = (w == 0) ? group : AgentStepLayout::Continuation;
                lines.push_back(format_agent_step(wrapped[w], layout));
            }
        } else {
            for (const std::string& wrapped : wrap_text(msg.text, static_cast<std::size_t>(inner))) {
                lines.push_back(styled_line(msg.role, wrapped));
            }
            lines.push_back(std::string());
        }
    }

    while (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}

void ChatPanel::rebuild_cache(int width) {
    render_cache_ = build_render_lines(width);
    cache_width_ = width;
    content_dirty_ = false;
}

void ChatPanel::begin_submit(std::string_view line, const EditorState& editor,
                             const std::string& workspace_root) {
    if (line.empty() || stream_active_ || !tutor_) {
        return;
    }

    const ParsedCommand cmd = parse_chat_command(line);
    const int prev_rows = static_cast<int>(render_cache_.size());
    messages_.push_back({MessageRole::User, std::string(line), cmd.mode});

    // Show an immediate spinner step so the user knows work is in progress
    // while the async online search (curl) runs in the background.
    messages_.push_back({MessageRole::Agent, "⠋ searching the web...", cmd.mode});

    TutorRequest req;
    req.user_text = std::string(line);
    req.command = cmd;
    req.editor = editor;
    req.workspace_root = workspace_root;

    pending_job_ = tutor_->begin_async(req);
    stream_active_ = true;
    job_fetching_ = true;
    fetch_spinner_tick_ = 0;
    response_started_ = false;
    header_dirty_ = true;
    mark_content_dirty(prev_rows);
    scroll_to_bottom();
}

bool ChatPanel::tick_stream() {
    if (!stream_active_ || !tutor_) {
        return false;
    }

    // ── Async fetch phase ────────────────────────────────────────────────────
    // Poll the future without blocking. While it is not ready, animate the
    // spinner on the last Agent message and return true (still "streaming").
    if (job_fetching_) {
        const auto status = pending_job_.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            stream_job_ = pending_job_.get();
            job_fetching_ = false;
            // Remove the placeholder spinner message; real steps will be
            // added by the normal tick path below.
            if (!messages_.empty() && messages_.back().role == MessageRole::Agent) {
                messages_.pop_back();
            }
            mark_all_dirty();
        } else {
            // Animate braille spinner at ~8 fps (each tick ≈ 12 ms in the run
            // loop which processes up to 8 ticks per frame).
            static const char* kFrames[] = {
                "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
            ++fetch_spinner_tick_;
            const int frame = (fetch_spinner_tick_ / 6) % 10;
            if (!messages_.empty() && messages_.back().role == MessageRole::Agent) {
                messages_.back().text =
                    std::string(kFrames[frame]) + " searching the web...";
                mark_content_dirty(std::max(0, static_cast<int>(render_cache_.size()) - 2));
            }
        }
        return true;
    }
    // ── Normal streaming phase ───────────────────────────────────────────────

    const int prev_rows = static_cast<int>(render_cache_.size());
    std::string step;
    std::string chunk;
    const bool more = tutor_->tick(stream_job_, step, chunk);

    bool changed = false;

    if (!step.empty()) {
        messages_.push_back({MessageRole::Agent, step, stream_job_.mode});
        mark_content_dirty(prev_rows);
        scroll_to_bottom();
        changed = true;
    }

    if (!chunk.empty()) {
        if (!response_started_) {
            messages_.push_back({MessageRole::Tutor, {}, stream_job_.mode});
            response_started_ = true;
            mark_content_dirty(static_cast<int>(render_cache_.size()) - 1);
        }
        messages_.back().text += chunk;
        mark_content_dirty(static_cast<int>(render_cache_.size()) - 1);
        scroll_to_bottom();
        changed = true;
    }

    if (!more && stream_job_.phase == TutorJob::Phase::Done) {
        stream_active_ = false;
        header_dirty_ = true;
        panel_dirty_ = true;
    }

    return changed;
}

void ChatPanel::render_header(Terminal& term, const SplitLayout& layout, AiMode active_mode) const {
    const int width = std::max(1, layout.chat.width);
    std::string header;
    header.append(kMuted);
    header.append("tutor");
    header.append(kReset);
    header.append(kStepSep);
    header.append(" · ");
    header.append(kReset);
    header.append(kHeaderMode);
    header.append(mode_label(active_mode));
    header.append(kReset);
    if (stream_active_) {
        header.append(kStepSep);
        header.append(" · ");
        header.append(kReset);
        header.append(kHeaderLive);
        header.append("● live");
        header.append(kReset);
    }

    term.move_cursor(layout.chat.top, layout.chat.left);
    term.write("\x1b[K");
    term.write(ansi_clip_visible(header, static_cast<std::size_t>(width)));
}

void ChatPanel::render_body(Terminal& term, const SplitLayout& layout, int body_rows) {
    const int header_rows = 1;
    const int width = std::max(1, layout.chat.width);
    const int total = static_cast<int>(render_cache_.size());
    scroll_offset_ = std::clamp(scroll_offset_, 0, std::max(0, total - body_rows));

    int start_viewport = 0;
    if (stream_active_ && scroll_offset_ == last_rendered_scroll_ && !content_dirty_) {
        start_viewport = std::max(0, dirty_from_cache_line_ - scroll_offset_);
    }
    start_viewport = std::min(start_viewport, body_rows);

    for (int i = start_viewport; i < body_rows; ++i) {
        const int idx = scroll_offset_ + i;
        term.move_cursor(layout.chat.top + header_rows + i, layout.chat.left);
        term.write("\x1b[K");  // erase from chat.left to end-of-line only
        if (idx >= 0 && idx < total) {
            term.write(ansi_clip_visible(render_cache_[static_cast<std::size_t>(idx)],
                                         static_cast<std::size_t>(width)));
        }
    }

    last_rendered_scroll_ = scroll_offset_;
    dirty_from_cache_line_ = total + 1;
}

void ChatPanel::render_input(Terminal& term, const SplitLayout& layout, bool focused,
                             std::string_view input_line) const {
    const int width = std::max(1, layout.chat.width);
    const int input_row = layout.chat.top + layout.chat.height - 1;
    constexpr int kPrefixCols = 2;  // "› "

    term.move_cursor(input_row, layout.chat.left);
    term.write("\x1b[K");  // erase from chat.left to end-of-line only
    if (focused) {
        term.write(kFocus);
    }
    term.write(kDim);
    term.write("› ");
    term.write(kReset);
    const std::string clipped = std::string(
        utf8_slice_by_chars(input_line, 0, static_cast<std::size_t>(width - kPrefixCols)));
    term.write(style_chat_tokens(clipped, kUserAccent));
    if (focused) {
        term.write(kReset);
    }

    if (focused && !stream_active_) {
        const int cursor_col =
            layout.chat.left + kPrefixCols +
            static_cast<int>(utf8_char_count(clipped));
        term.move_cursor(input_row, cursor_col);
    }
}

void ChatPanel::render_mention_menu(Terminal& term, const SplitLayout& layout,
                                    const MentionMenu& menu) const {
    if (!menu.active || menu.candidates.empty()) {
        return;
    }

    const int rows = std::min(static_cast<int>(menu.candidates.size()), 5);
    const int width = std::max(1, layout.chat.width - 2);
    const int top = layout.chat.top + layout.chat.height - 2 - rows;

    for (int i = 0; i < rows; ++i) {
        term.move_cursor(top + i, layout.chat.left);
        term.write("\x1b[K");  // erase from chat.left to end-of-line only
        const bool sel = i == menu.selected;
        if (sel) {
            term.write(kFocus);
        }
        term.write(kPick);
        term.write("@");
        term.write(fs::path(menu.candidates[static_cast<std::size_t>(i)]).filename().string());
        term.write(kDim);
        const std::string dir = fs::path(menu.candidates[static_cast<std::size_t>(i)]).parent_path().string();
        const std::string clipped =
            std::string(utf8_slice_by_chars(dir, 0, static_cast<std::size_t>(width - 20)));
        term.write("  ");
        term.write(clipped);
        term.write(kReset);
    }
}

void ChatPanel::render(Terminal& term, const SplitLayout& layout, bool focused,
                       std::string_view input_line, AiMode active_mode,
                       const MentionMenu* mention) {
    const bool resized =
        layout.chat.width != last_width_ || layout.chat.height != last_height_;

    if (panel_dirty_ || resized) {
        term.clear_rect(layout.chat);
        panel_dirty_ = false;
        content_dirty_ = true;
        header_dirty_ = true;
        dirty_from_cache_line_ = 0;
        last_rendered_scroll_ = -1;
        last_width_ = layout.chat.width;
        last_height_ = layout.chat.height;
    }

    const int body_rows = std::max(1, layout.chat.height - 2);
    const int width = std::max(1, layout.chat.width);

    if (content_dirty_ || cache_width_ != width) {
        rebuild_cache(width);
    }

    if (header_dirty_) {
        render_header(term, layout, active_mode);
        header_dirty_ = false;
    }

    render_body(term, layout, body_rows);

    if (mention != nullptr) {
        render_mention_menu(term, layout, *mention);
    }

    render_input(term, layout, focused, input_line);

    if (focused && !stream_active_) {
        term.show_cursor();
    }
}

}  // namespace askdocs
