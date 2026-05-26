#pragma once

#include "askdocs/types.hpp"

#include <string>
#include <string_view>

namespace askdocs {

class Terminal {
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    bool is_tty() const noexcept { return is_tty_; }
    Size size() const noexcept { return size_; }
    void poll_size();

    void write(std::string_view text);
    void flush();

    void hide_cursor();
    void show_cursor();
    void move_cursor(int row, int col);
    void clear_screen();
    void clear_rect(const Rect& rect);
    void set_scroll_region(int top, int bottom);
    void reset_scroll_region();

    void enter_alternate_screen();
    void leave_alternate_screen();

    void enable_mouse();
    void disable_mouse();

    bool read_key(std::string& out_key);

private:
    bool is_tty_ = false;
    Size size_{24, 80};
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;

#if defined(_WIN32)
    void* original_mode_ = nullptr;
#else
    void* original_mode_ = nullptr;  // struct termios, owned in terminal.cpp
#endif
};

}  // namespace askdocs
