#pragma once

#include "askdocs/command.hpp"
#include "askdocs/layout.hpp"
#include "askdocs/terminal.hpp"
#include "askdocs/tutor_service.hpp"
#include "askdocs/types.hpp"

#include <chrono>
#include <deque>
#include <future>
#include <string>
#include <vector>

namespace askdocs {

struct MentionMenu {
    bool active = false;
    std::size_t at_byte = 0;
    std::string query;
    int selected = 0;
    std::vector<std::string> candidates;
};

class ChatPanel {
public:
    ChatPanel();

    void render(Terminal& term, const SplitLayout& layout, bool focused,
                std::string_view input_line, AiMode active_mode,
                const MentionMenu* mention = nullptr);
    void mark_all_dirty() noexcept;

    const std::deque<ChatMessage>& messages() const noexcept { return messages_; }

    int scroll_offset() const noexcept { return scroll_offset_; }
    void scroll_up(int lines = 1) noexcept;
    void scroll_down(int lines = 1) noexcept;
    void scroll_to_bottom() noexcept;

    void begin_submit(std::string_view line, const EditorState& editor,
                      const std::string& workspace_root);
    bool tick_stream();
    bool is_streaming() const noexcept { return stream_active_; }

    void add_system(std::string text);
    void set_tutor(TutorService* tutor) noexcept { tutor_ = tutor; }

private:
    TutorService* tutor_ = nullptr;
    std::deque<ChatMessage> messages_;
    int scroll_offset_ = 0;
    bool panel_dirty_ = true;
    bool content_dirty_ = true;
    bool header_dirty_ = true;
    int last_width_ = -1;
    int last_height_ = -1;

    bool stream_active_ = false;
    bool job_fetching_ = false;          // true while begin_async future is pending
    std::future<TutorJob> pending_job_;  // live during async fetch phase
    int fetch_spinner_tick_ = 0;         // drives the braille spinner animation
    TutorJob stream_job_;
    bool response_started_ = false;

    std::vector<std::string> render_cache_;
    int cache_width_ = -1;
    int dirty_from_cache_line_ = 0;
    int last_rendered_scroll_ = -1;

    void rebuild_cache(int width);
    void mark_content_dirty(int from_row = 0) noexcept;
    std::vector<std::string> build_render_lines(int width) const;
    void render_header(Terminal& term, const SplitLayout& layout, AiMode active_mode) const;
    void render_body(Terminal& term, const SplitLayout& layout, int body_rows);
    void render_input(Terminal& term, const SplitLayout& layout, bool focused,
                      std::string_view input_line) const;
    void render_mention_menu(Terminal& term, const SplitLayout& layout,
                             const MentionMenu& menu) const;
};

}  // namespace askdocs
