#pragma once

#include "askdocs/types.hpp"

#include <string>

namespace askdocs {

struct InputAction {
    enum class Kind {
        None,
        Quit,
        SwitchFocus,
        SaveFile,
        EditorMove,
        EditorInsert,
        EditorBackspace,
        EditorDelete,
        EditorNewline,
        ExplorerMove,
        ExplorerOpen,
        ExplorerCollapse,
        ExplorerParent,
        ExplorerToggleHidden,
        ExplorerRefresh,
        ChatInsert,
        ChatBackspace,
        ChatSubmit,
        ChatScrollUp,
        ChatScrollDown,
        ChatMentionUp,
        ChatMentionDown,
        ChatMentionAccept,
        EditorScrollUp,
        EditorScrollDown,
        Resize,
        ToggleMode,
    };

    Kind kind = Kind::None;
    Point delta{0, 0};
    char ch = '\0';
    AiMode mode = AiMode::Learn;
    bool mention_active = false;
};

class InputHandler {
public:
    InputAction handle(FocusTarget focus, std::string_view key, bool mention_menu_active = false);
};

}  // namespace askdocs
