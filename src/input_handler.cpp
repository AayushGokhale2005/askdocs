#include "askdocs/input_handler.hpp"

namespace askdocs {

InputAction InputHandler::handle(FocusTarget focus, std::string_view key, bool mention_menu_active) {
    InputAction action;

    if (key == "\x04") {  // Ctrl-D quit
        action.kind = InputAction::Kind::Quit;
        return action;
    }

    if (key == "\x13") {  // Ctrl-S save
        action.kind = InputAction::Kind::SaveFile;
        return action;
    }

    if (key == "\t") {
        action.kind = InputAction::Kind::SwitchFocus;
        return action;
    }

    // Mouse wheel scroll events (SGR, normalised by Terminal::read_key).
    // These are intercepted before focus-specific key maps so that scroll
    // always works regardless of which panel is focused.
    if (key == "\x1b[SCROLL_UP]") {
        if (focus == FocusTarget::Editor) {
            action.kind = InputAction::Kind::EditorScrollUp;
        } else if (focus == FocusTarget::Chat) {
            action.kind = InputAction::Kind::ChatScrollUp;
        } else {
            // Explorer — reuse ExplorerMove upward
            action.kind = InputAction::Kind::ExplorerMove;
            action.delta = {-3, 0};
        }
        return action;
    }
    if (key == "\x1b[SCROLL_DOWN]") {
        if (focus == FocusTarget::Editor) {
            action.kind = InputAction::Kind::EditorScrollDown;
        } else if (focus == FocusTarget::Chat) {
            action.kind = InputAction::Kind::ChatScrollDown;
        } else {
            action.kind = InputAction::Kind::ExplorerMove;
            action.delta = {3, 0};
        }
        return action;
    }

    if (focus == FocusTarget::Explorer) {
        if (key == "\x1b[A") {
            action.kind = InputAction::Kind::ExplorerMove;
            action.delta = {-1, 0};
            return action;
        }
        if (key == "\x1b[B") {
            action.kind = InputAction::Kind::ExplorerMove;
            action.delta = {1, 0};
            return action;
        }
        if (key == "\r" || key == "\n" || key == "\x1b[C") {
            action.kind = InputAction::Kind::ExplorerOpen;
            return action;
        }
        if (key == "\x1b[D") {
            action.kind = InputAction::Kind::ExplorerCollapse;
            return action;
        }
        if (key == "\x7f" || key == "\b" || key == "-") {
            action.kind = InputAction::Kind::ExplorerParent;
            return action;
        }
        if (key == "a") {
            action.kind = InputAction::Kind::ExplorerToggleHidden;
            return action;
        }
        if (key == "r") {
            action.kind = InputAction::Kind::ExplorerRefresh;
            return action;
        }
        if (key == "\x1b[5~") {
            action.kind = InputAction::Kind::ExplorerMove;
            action.delta = {-5, 0};
            return action;
        }
        if (key == "\x1b[6~") {
            action.kind = InputAction::Kind::ExplorerMove;
            action.delta = {5, 0};
            return action;
        }
        return action;
    }

    if (key == "\x1b[5~") {
        action.kind = InputAction::Kind::ChatScrollUp;
        return action;
    }
    if (key == "\x1b[6~") {
        action.kind = InputAction::Kind::ChatScrollDown;
        return action;
    }

    if (focus == FocusTarget::Editor) {
        if (key == "\x1b[A") {
            action.kind = InputAction::Kind::EditorMove;
            action.delta = {-1, 0};
            return action;
        }
        if (key == "\x1b[B") {
            action.kind = InputAction::Kind::EditorMove;
            action.delta = {1, 0};
            return action;
        }
        if (key == "\x1b[C") {
            action.kind = InputAction::Kind::EditorMove;
            action.delta = {0, 1};
            return action;
        }
        if (key == "\x1b[D") {
            action.kind = InputAction::Kind::EditorMove;
            action.delta = {0, -1};
            return action;
        }
        if (key == "\x7f" || key == "\b") {
            action.kind = InputAction::Kind::EditorBackspace;
            return action;
        }
        if (key == "\x1b[3~") {
            action.kind = InputAction::Kind::EditorDelete;
            return action;
        }
        if (key == "\r" || key == "\n") {
            action.kind = InputAction::Kind::EditorNewline;
            return action;
        }
        if (key.size() == 1 && key[0] >= 32 && key[0] != 127) {
            action.kind = InputAction::Kind::EditorInsert;
            action.ch = key[0];
            return action;
        }
    } else if (focus == FocusTarget::Chat) {
        if (mention_menu_active) {
            if (key == "\x1b[A") {
                action.kind = InputAction::Kind::ChatMentionUp;
                return action;
            }
            if (key == "\x1b[B") {
                action.kind = InputAction::Kind::ChatMentionDown;
                return action;
            }
            if (key == "\t" || key == "\r" || key == "\n") {
                action.kind = InputAction::Kind::ChatMentionAccept;
                return action;
            }
        }
        if (key == "\x1b[A") {
            action.kind = InputAction::Kind::ChatScrollUp;
            return action;
        }
        if (key == "\x1b[B") {
            action.kind = InputAction::Kind::ChatScrollDown;
            return action;
        }
        if (key == "\x7f" || key == "\b") {
            action.kind = InputAction::Kind::ChatBackspace;
            return action;
        }
        if (key == "\r" || key == "\n") {
            action.kind = InputAction::Kind::ChatSubmit;
            return action;
        }
        if (key.size() == 1 && key[0] >= 32 && key[0] != 127) {
            action.kind = InputAction::Kind::ChatInsert;
            action.ch = key[0];
            return action;
        }
    }

    return action;
}

}  // namespace askdocs
