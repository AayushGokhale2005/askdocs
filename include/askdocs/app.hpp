#pragma once

#include "askdocs/app_options.hpp"
#include "askdocs/chat_panel.hpp"
#include "askdocs/editor_view.hpp"
#include "askdocs/explorer.hpp"
#include "askdocs/file_io.hpp"
#include "askdocs/input_handler.hpp"
#include "askdocs/layout.hpp"
#include "askdocs/terminal.hpp"
#include "askdocs/tutor_service.hpp"
#include "askdocs/types.hpp"

namespace askdocs {

class App {
public:
    explicit App(AppOptions options = {});

    int run();

private:
    AppOptions options_;
    Terminal term_;
    SplitLayout layout_;
    EditorState editor_;
    EditorView editor_view_;
    ExplorerPanel explorer_;
    ChatPanel chat_;
    TutorService tutor_;
    InputHandler input_;
    FocusTarget focus_ = FocusTarget::Editor;
    AiMode ai_mode_ = AiMode::Learn;
    std::string chat_input_;
    MentionMenu mention_;
    bool running_ = true;
    bool needs_redraw_ = true;
    bool force_clear_next_render_ = true;

    void open_file(const std::string& path);
    void new_empty_buffer();
    void handle_action(const InputAction& action);
    void render();
    void render_chat_only();
    void render_status_bar();
    void clamp_cursor();
    void ensure_cursor_visible();
    void insert_char(char ch);
    void backspace();
    void delete_forward();
    void move_cursor(int drow, int dcol);
    void save_current_file();
    void cycle_focus();
    std::string focus_label() const;
    std::string mode_label(AiMode mode) const;
    void update_mention_menu();
    void apply_mention(const std::string& path);
    std::string workspace_root() const;
};

}  // namespace askdocs
