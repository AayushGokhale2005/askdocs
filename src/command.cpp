#include "askdocs/command.hpp"

namespace askdocs {

ParsedCommand parse_chat_command(std::string_view input) {
    ParsedCommand cmd;
    if (input.empty() || input[0] != '/') {
        cmd.payload = std::string(input);
        return cmd;
    }

    cmd.is_command = true;
    const std::size_t space = input.find(' ');
    const std::size_t name_len =
        space == std::string_view::npos ? input.size() - 1 : space - 1;
    const std::string_view name = input.substr(1, name_len);
    cmd.command_name = std::string(name);
    cmd.payload = space == std::string_view::npos ? "" : std::string(input.substr(space + 1));

    if (name == "hint" || name == "step") {
        cmd.mode = AiMode::Learn;
    } else if (name == "debug" || name == "trace") {
        cmd.mode = AiMode::Debug;
    } else if (name == "explain") {
        cmd.mode = AiMode::Explain;
    } else if (name == "quiz") {
        cmd.mode = AiMode::Quiz;
    } else if (name == "override") {
        cmd.mode = AiMode::Override;
    } else if (name == "path") {
        cmd.mode = AiMode::Learn;
    } else if (name == "learn") {
        cmd.mode = AiMode::Learn;
    }

    return cmd;
}

}  // namespace askdocs
