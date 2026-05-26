#include "askdocs/app.hpp"
#include "askdocs/command.hpp"
#include "askdocs/syntax.hpp"
#include "askdocs/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace askdocs {

App::App(AppOptions options)
    : options_(std::move(options)),
      explorer_(options_.working_directory.empty() ? get_working_directory()
                                                   : options_.working_directory),
      tutor_(options_.working_directory.empty() ? get_working_directory()
                                                 : options_.working_directory) {
    explorer_.set_on_open([this](const std::string& path) { open_file(path); });
    chat_.set_tutor(&tutor_);

    if (!tutor_.ready()) {
        chat_.add_system("online doc search disabled — unset ASKDOCS_ONLINE=0");
    }

    if (!options_.initial_file.empty()) {
        open_file(resolve_path(options_.working_directory, options_.initial_file));
    } else {
        new_empty_buffer();
    }

    chat_.add_system("AskDocs ready — online docs tutor active");
    tutor_.refresh_index();
}

std::string App::workspace_root() const {
    return explorer_.root_path();
}

void App::update_mention_menu() {
    mention_.active = false;
    mention_.candidates.clear();
    mention_.query.clear();
    mention_.selected = 0;

    const std::size_t at = chat_input_.rfind('@');
    if (at == std::string::npos) {
        return;
    }
    if (at > 0 && !std::isspace(static_cast<unsigned char>(chat_input_[at - 1]))) {
        return;
    }

    const std::string query = chat_input_.substr(at + 1);
    if (query.find(' ') != std::string::npos) {
        return;
    }

    mention_.active = true;
    mention_.at_byte = at;
    mention_.query = query;
    mention_.candidates = tutor_.file_index().complete(query, 8);
    if (mention_.selected >= static_cast<int>(mention_.candidates.size())) {
        mention_.selected = 0;
    }
}

void App::apply_mention(const std::string& path) {
    const std::string fname = fs::path(path).filename().string();
    chat_input_ = chat_input_.substr(0, mention_.at_byte) + "@" + fname;
    mention_.active = false;
    mention_.candidates.clear();
    needs_redraw_ = true;
}

void App::new_empty_buffer() {
    editor_.lines = {""};
    editor_.cursor = {0, 0};
    editor_.scroll_row = 0;
    editor_.filepath = "untitled";
    editor_.language = Language::Generic;
    editor_.dirty = false;
}

void App::open_file(const std::string& path) {
    const LoadedFile loaded = load_file(path);
    if (!loaded.ok) {
        chat_.add_system("Failed to open: " + loaded.error);
        needs_redraw_ = true;
        return;
    }

    editor_.lines = loaded.lines;
    editor_.filepath = loaded.path;
    editor_.language = loaded.language;
    editor_.cursor = {0, 0};
    editor_.scroll_row = 0;
    editor_.dirty = false;
    focus_ = FocusTarget::Editor;

    std::error_code ec;
    const fs::path parent = fs::path(loaded.path).parent_path();
    if (!parent.empty()) {
        explorer_.set_root(parent.string());
        tutor_.refresh_index();
    }

    chat_.add_system("Opened " + loaded.path + " [" + language_name(loaded.language) + "]");
    editor_view_.mark_all_dirty();
    needs_redraw_ = true;
}

void App::save_current_file() {
    if (editor_.filepath == "untitled") {
        chat_.add_system("Save requires a file path. Open a file from the explorer first.");
        needs_redraw_ = true;
        return;
    }

    std::string error;
    if (!save_file(editor_.filepath, editor_.lines, error)) {
        chat_.add_system("Save failed: " + error);
    } else {
        editor_.dirty = false;
        chat_.add_system("Saved " + editor_.filepath);
    }
    needs_redraw_ = true;
}

std::string App::focus_label() const {
    switch (focus_) {
        case FocusTarget::Explorer:
            return "FILES";
        case FocusTarget::Editor:
            return "EDITOR";
        case FocusTarget::Chat:
            return "CHAT";
    }
    return "EDITOR";
}

std::string App::mode_label(AiMode mode) const {
    switch (mode) {
        case AiMode::Learn:
            return "LEARN";
        case AiMode::Debug:
            return "DEBUG";
        case AiMode::Explain:
            return "EXPLAIN";
        case AiMode::Quiz:
            return "QUIZ";
        case AiMode::Override:
            return "OVERRIDE";
    }
    return "LEARN";
}

void App::cycle_focus() {
    switch (focus_) {
        case FocusTarget::Explorer:
            focus_ = FocusTarget::Editor;
            break;
        case FocusTarget::Editor:
            focus_ = FocusTarget::Chat;
            break;
        case FocusTarget::Chat:
            focus_ = FocusTarget::Explorer;
            break;
    }
    needs_redraw_ = true;
}

void App::clamp_cursor() {
    if (editor_.lines.empty()) {
        editor_.lines.emplace_back();
    }
    editor_.cursor.row =
        std::clamp(editor_.cursor.row, 0, std::max(0, static_cast<int>(editor_.lines.size()) - 1));
    const std::string& line = editor_.lines[static_cast<std::size_t>(editor_.cursor.row)];
    const int len = static_cast<int>(utf8_char_count(line));
    editor_.cursor.col = std::clamp(editor_.cursor.col, 0, len);
}

void App::ensure_cursor_visible() {
    const int visible = std::max(1, layout_.editor_content.height - 2);
    if (editor_.cursor.row < editor_.scroll_row) {
        editor_.scroll_row = editor_.cursor.row;
    } else if (editor_.cursor.row >= editor_.scroll_row + visible) {
        editor_.scroll_row = editor_.cursor.row - visible + 1;
    }
}

void App::move_cursor(int drow, int dcol) {
    editor_.cursor.row += drow;
    editor_.cursor.col += dcol;
    clamp_cursor();
    ensure_cursor_visible();
    needs_redraw_ = true;
}

void App::insert_char(char ch) {
    std::string& line = editor_.lines[static_cast<std::size_t>(editor_.cursor.row)];
    const std::size_t byte_idx =
        utf8_byte_index_for_char(line, static_cast<std::size_t>(editor_.cursor.col));
    line.insert(byte_idx, 1, ch);
    editor_.cursor.col += 1;
    editor_.dirty = true;
    needs_redraw_ = true;
}

void App::backspace() {
    if (editor_.cursor.col > 0) {
        std::string& line = editor_.lines[static_cast<std::size_t>(editor_.cursor.row)];
        const std::size_t byte_idx =
            utf8_byte_index_for_char(line, static_cast<std::size_t>(editor_.cursor.col - 1));
        const std::size_t next =
            utf8_byte_index_for_char(line, static_cast<std::size_t>(editor_.cursor.col));
        line.erase(byte_idx, next - byte_idx);
        editor_.cursor.col -= 1;
    } else if (editor_.cursor.row > 0) {
        std::string& cur = editor_.lines[static_cast<std::size_t>(editor_.cursor.row)];
        std::string& prev = editor_.lines[static_cast<std::size_t>(editor_.cursor.row - 1)];
        editor_.cursor.col = static_cast<int>(utf8_char_count(prev));
        prev += cur;
        editor_.lines.erase(editor_.lines.begin() + editor_.cursor.row);
        editor_.cursor.row -= 1;
    }
    editor_.dirty = true;
    needs_redraw_ = true;
}

void App::delete_forward() {
    std::string& line = editor_.lines[static_cast<std::size_t>(editor_.cursor.row)];
    if (editor_.cursor.col < static_cast<int>(utf8_char_count(line))) {
        const std::size_t byte_idx =
            utf8_byte_index_for_char(line, static_cast<std::size_t>(editor_.cursor.col));
        const std::size_t next =
            utf8_byte_index_for_char(line, static_cast<std::size_t>(editor_.cursor.col + 1));
        line.erase(byte_idx, next - byte_idx);
    } else if (editor_.cursor.row + 1 < static_cast<int>(editor_.lines.size())) {
        line += editor_.lines[static_cast<std::size_t>(editor_.cursor.row + 1)];
        editor_.lines.erase(editor_.lines.begin() + editor_.cursor.row + 1);
    }
    editor_.dirty = true;
    needs_redraw_ = true;
}

void App::handle_action(const InputAction& action) {
    switch (action.kind) {
        case InputAction::Kind::None:
            break;
        case InputAction::Kind::Quit:
            running_ = false;
            break;
        case InputAction::Kind::SwitchFocus:
            cycle_focus();
            break;
        case InputAction::Kind::SaveFile:
            save_current_file();
            break;
        case InputAction::Kind::EditorMove:
            move_cursor(action.delta.row, action.delta.col);
            break;
        case InputAction::Kind::EditorInsert:
            insert_char(action.ch);
            break;
        case InputAction::Kind::EditorBackspace:
            backspace();
            break;
        case InputAction::Kind::EditorDelete:
            delete_forward();
            break;
        case InputAction::Kind::EditorNewline: {
            std::string& line = editor_.lines[static_cast<std::size_t>(editor_.cursor.row)];
            const std::size_t byte_idx =
                utf8_byte_index_for_char(line, static_cast<std::size_t>(editor_.cursor.col));
            std::string tail = line.substr(byte_idx);
            line.erase(byte_idx);
            editor_.lines.insert(editor_.lines.begin() + editor_.cursor.row + 1, std::move(tail));
            editor_.cursor.row += 1;
            editor_.cursor.col = 0;
            editor_.dirty = true;
            ensure_cursor_visible();
            needs_redraw_ = true;
            break;
        }
        case InputAction::Kind::ExplorerMove:
            if (action.delta.row < 0) {
                for (int i = 0; i < -action.delta.row; ++i) {
                    explorer_.move_up();
                }
            } else if (action.delta.row > 0) {
                for (int i = 0; i < action.delta.row; ++i) {
                    explorer_.move_down();
                }
            }
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ExplorerOpen:
            explorer_.open_or_expand();
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ExplorerCollapse:
            explorer_.collapse_or_parent();
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ExplorerParent:
            explorer_.go_to_parent();
            tutor_.refresh_index();
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ExplorerToggleHidden:
            explorer_.toggle_hidden();
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ExplorerRefresh:
            explorer_.refresh();
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ChatInsert:
            if (!chat_.is_streaming()) {
                chat_input_.push_back(action.ch);
                update_mention_menu();
            }
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ChatBackspace:
            if (!chat_.is_streaming() && !chat_input_.empty()) {
                chat_input_.pop_back();
                update_mention_menu();
            }
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ChatSubmit:
            if (!chat_.is_streaming()) {
                chat_.begin_submit(chat_input_, editor_, workspace_root());
                const ParsedCommand cmd = parse_chat_command(chat_input_);
                if (cmd.is_command) {
                    ai_mode_ = cmd.mode;
                }
                chat_input_.clear();
                mention_.active = false;
            }
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ChatMentionUp:
            if (mention_.active && !mention_.candidates.empty()) {
                mention_.selected = std::max(0, mention_.selected - 1);
            }
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ChatMentionDown:
            if (mention_.active && !mention_.candidates.empty()) {
                mention_.selected =
                    std::min(static_cast<int>(mention_.candidates.size()) - 1, mention_.selected + 1);
            }
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ChatMentionAccept:
            if (mention_.active && !mention_.candidates.empty()) {
                apply_mention(mention_.candidates[static_cast<std::size_t>(mention_.selected)]);
            }
            break;
        case InputAction::Kind::ChatScrollUp:
            chat_.scroll_up(3);
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ChatScrollDown:
            chat_.scroll_down(3);
            needs_redraw_ = true;
            break;
        case InputAction::Kind::EditorScrollUp:
            editor_.scroll_row = std::max(0, editor_.scroll_row - 3);
            ensure_cursor_visible();
            editor_view_.mark_all_dirty();
            needs_redraw_ = true;
            break;
        case InputAction::Kind::EditorScrollDown:
            editor_.scroll_row =
                std::min(std::max(0, static_cast<int>(editor_.lines.size()) - 1),
                         editor_.scroll_row + 3);
            ensure_cursor_visible();
            editor_view_.mark_all_dirty();
            needs_redraw_ = true;
            break;
        case InputAction::Kind::Resize:
            layout_ = compute_layout(term_.size());
            editor_view_.mark_all_dirty();
            chat_.mark_all_dirty();
            explorer_.mark_all_dirty();
            ensure_cursor_visible();
            force_clear_next_render_ = true;
            needs_redraw_ = true;
            break;
        case InputAction::Kind::ToggleMode:
            break;
    }
}

void App::render_status_bar() {
    std::string bar;
    bar += " AskDocs ";
    bar += focus_label();
    bar += " | ";
    bar += editor_.filepath;
    bar += editor_.dirty ? " [+]" : "    ";
    bar += " | ";
    bar += language_name(editor_.language);
    bar += " | AI:";
    bar += mode_label(ai_mode_);
    bar += " | Tab focus | Ctrl-S save | Ctrl-D quit ";

    const int width = std::max(1, layout_.status.width);
    term_.move_cursor(layout_.status.top, layout_.status.left);
    term_.write("\x1b[2K");
    term_.write("\x1b[48;5;235m");
    term_.write(utf8_slice_by_chars(bar, 0, static_cast<std::size_t>(width)));
    term_.write("\x1b[0m");
}

void App::render_chat_only() {
    term_.hide_cursor();
    chat_.render(term_, layout_, focus_ == FocusTarget::Chat, chat_input_, ai_mode_,
                 mention_.active ? &mention_ : nullptr);
    render_status_bar();
    term_.flush();
}

void App::render() {
    term_.hide_cursor();

    if (force_clear_next_render_) {
        term_.clear_screen();
        editor_view_.mark_all_dirty();
        chat_.mark_all_dirty();
        explorer_.mark_all_dirty();
        force_clear_next_render_ = false;
    }

    explorer_.render(term_, layout_.explorer, focus_ == FocusTarget::Explorer);

    for (int r = 0; r < layout_.editor.height; ++r) {
        term_.move_cursor(layout_.explorer.top + r, layout_.explorer.left + layout_.explorer.width);
        term_.write("\x1b[90m│\x1b[0m");
    }

    editor_view_.set_state(editor_);
    editor_view_.render(term_, layout_, focus_ == FocusTarget::Editor);

    for (int r = 0; r < layout_.chat.height; ++r) {
        term_.move_cursor(layout_.chat.top + r, layout_.editor.left + layout_.editor.width);
        term_.write("\x1b[90m│\x1b[0m");
    }

    chat_.render(term_, layout_, focus_ == FocusTarget::Chat, chat_input_, ai_mode_,
                 mention_.active ? &mention_ : nullptr);
    render_status_bar();
    term_.flush();
}

int App::run() {
    if (!term_.is_tty()) {
        return 1;
    }

    term_.poll_size();
    layout_ = compute_layout(term_.size());
    term_.poll_size();
    layout_ = compute_layout(term_.size());
    force_clear_next_render_ = true;
    needs_redraw_ = true;
    render();
    needs_redraw_ = false;

    while (running_) {
        const Size prev = term_.size();
        term_.poll_size();
        if (term_.size().rows != prev.rows || term_.size().cols != prev.cols) {
            handle_action(InputAction{InputAction::Kind::Resize});
        }

        if (chat_.is_streaming()) {
            bool changed = false;
            for (int i = 0; i < 8 && chat_.is_streaming(); ++i) {
                changed |= chat_.tick_stream();
            }
            if (!chat_.is_streaming()) {
                force_clear_next_render_ = true;
                needs_redraw_ = true;
            } else if (changed) {
                render_chat_only();
            }
        }

        if (needs_redraw_) {
            render();
            needs_redraw_ = false;
        }

        std::string key;
        if (!term_.read_key(key)) {
            continue;
        }

        handle_action(input_.handle(focus_, key, mention_.active));

        if (needs_redraw_) {
            render();
            needs_redraw_ = false;
        }
    }

    return 0;
}

}  // namespace askdocs
