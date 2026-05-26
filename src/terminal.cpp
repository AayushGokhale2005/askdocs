#include "askdocs/terminal.hpp"

#include <array>
#include <cctype>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <io.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace askdocs {

Terminal::Terminal() {
#if defined(_WIN32)
    stdin_fd_ = _fileno(stdin);
    stdout_fd_ = _fileno(stdout);
#else
    stdin_fd_ = STDIN_FILENO;
    stdout_fd_ = STDOUT_FILENO;
#endif

    is_tty_ = ::isatty(stdin_fd_) && ::isatty(stdout_fd_);
    if (!is_tty_) {
        return;
    }

#if !defined(_WIN32)
    termios raw{};
    if (tcgetattr(stdin_fd_, &raw) != 0) {
        is_tty_ = false;
        return;
    }

    original_mode_ = new termios(raw);
    termios mode = raw;
    mode.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    mode.c_oflag &= ~(OPOST);
    mode.c_cflag |= (CS8);
    mode.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    mode.c_cc[VMIN] = 0;
    mode.c_cc[VTIME] = 1;
    if (tcsetattr(stdin_fd_, TCSAFLUSH, &mode) != 0) {
        delete static_cast<termios*>(original_mode_);
        original_mode_ = nullptr;
        original_mode_ = nullptr;
        is_tty_ = false;
        return;
    }
#endif

    enter_alternate_screen();
    clear_screen();
    hide_cursor();
    enable_mouse();
    poll_size();
}

Terminal::~Terminal() {
    if (!is_tty_) {
        return;
    }

    disable_mouse();
    leave_alternate_screen();
    show_cursor();
    flush();

#if !defined(_WIN32)
    if (original_mode_) {
        tcsetattr(stdin_fd_, TCSAFLUSH, static_cast<termios*>(original_mode_));
        delete static_cast<termios*>(original_mode_);
        original_mode_ = nullptr;
    }
#endif
}

void Terminal::poll_size() {
#if defined(_WIN32)
    // Minimal fallback for non-Unix builds.
    size_.rows = 24;
    size_.cols = 80;
#else
    winsize ws{};
    if (ioctl(stdout_fd_, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        size_.rows = ws.ws_row;
        size_.cols = ws.ws_col;
    }
#endif
}

void Terminal::write(std::string_view text) {
    if (text.empty()) {
        return;
    }
#if defined(_WIN32)
    std::fwrite(text.data(), 1, text.size(), stdout);
#else
    ::write(stdout_fd_, text.data(), text.size());
#endif
}

void Terminal::flush() {
    std::fflush(stdout);
}

void Terminal::hide_cursor() { write("\x1b[?25l"); }

void Terminal::show_cursor() { write("\x1b[?25h"); }

void Terminal::move_cursor(int row, int col) {
    write("\x1b[");
    write(std::to_string(row + 1));
    write(";");
    write(std::to_string(col + 1));
    write("H");
}

void Terminal::clear_screen() { write("\x1b[2J\x1b[H"); }

void Terminal::clear_rect(const Rect& rect) {
    for (int r = 0; r < rect.height; ++r) {
        move_cursor(rect.top + r, rect.left);
        write("\x1b[K");
        if (rect.width > 0) {
            write(std::string(static_cast<std::size_t>(rect.width), ' '));
        }
    }
}

void Terminal::set_scroll_region(int top, int bottom) {
    write("\x1b[");
    write(std::to_string(top + 1));
    write(";");
    write(std::to_string(bottom + 1));
    write("r");
}

void Terminal::reset_scroll_region() { write("\x1b[r"); }

void Terminal::enter_alternate_screen() { write("\x1b[?1049h"); }

void Terminal::leave_alternate_screen() { write("\x1b[?1049l"); }

// Enable button-event mouse reporting + SGR extended coordinates (handles cols > 223).
void Terminal::enable_mouse() { write("\x1b[?1000h\x1b[?1006h"); }

void Terminal::disable_mouse() { write("\x1b[?1000l\x1b[?1006l"); }

bool Terminal::read_key(std::string& out_key) {
    out_key.clear();
    if (!is_tty_) {
        return false;
    }

    char buf[32];
#if defined(_WIN32)
    const std::size_t n = std::fread(buf, 1, sizeof(buf), stdin);
#else
    const ssize_t n = ::read(stdin_fd_, buf, sizeof(buf));
#endif
    if (n <= 0) {
        return false;
    }

    out_key.assign(buf, static_cast<std::size_t>(n));

    if (out_key == "\x1b") {
        char extra[16];
#if defined(_WIN32)
        const std::size_t extra_n = std::fread(extra, 1, sizeof(extra), stdin);
#else
        const ssize_t extra_n = ::read(stdin_fd_, extra, sizeof(extra));
#endif
        if (extra_n > 0) {
            out_key.append(extra, static_cast<std::size_t>(extra_n));
        }
    }

    // Detect SGR mouse sequence: ESC [ < Pb ; Px ; Py M|m
    // Scroll-up button = 64, scroll-down button = 65.
    if (out_key.size() >= 6 && out_key[0] == '\x1b' && out_key[1] == '[' && out_key[2] == '<') {
        int btn = 0;
        std::size_t pos = 3;
        while (pos < out_key.size() && std::isdigit(static_cast<unsigned char>(out_key[pos]))) {
            btn = btn * 10 + (out_key[pos] - '0');
            ++pos;
        }
        if (btn == 64) {
            out_key = "\x1b[SCROLL_UP]";
            return true;
        }
        if (btn == 65) {
            out_key = "\x1b[SCROLL_DOWN]";
            return true;
        }
        // Other mouse events (click, drag, release) — discard.
        out_key.clear();
        return false;
    }

    return true;
}

}  // namespace askdocs
