#pragma once

#include "askdocs/types.hpp"

#include <string>
#include <string_view>

namespace askdocs {

struct ParsedCommand {
    bool is_command = false;
    AiMode mode = AiMode::Learn;
    std::string payload;
    std::string command_name;
};

ParsedCommand parse_chat_command(std::string_view input);

}  // namespace askdocs
