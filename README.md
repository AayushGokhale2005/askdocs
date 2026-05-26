# AskDocs

A terminal-based, learning-first code editor. The AI layer is a Socratic tutor — not an autocomplete engine.

**Current scope:** UI prototype with file explorer, syntax highlighting, CLI file opening, split editor + chat panel.

## Install (use like `vim`)

### Homebrew (recommended)

```bash
brew tap AayushGokhale2005/tap
brew install askdocs
```

### From source

**One-time setup** so `askdocs` works in Mac Terminal, iTerm, etc.:

```bash
./scripts/install.sh
source ~/.zshrc
```

Or manually:

```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.local
export PATH="$HOME/.local/bin:$PATH"   # add this line to ~/.zshrc
```

Then open files the same way you would with vim:

```bash
askdocs src/main.cpp
askdocs README.md
askdocs                    # start with empty buffer + file explorer
```

Online doc search uses `curl` (built into macOS). Requires network for `/learn`, `/explain`, etc. C/C++ docs come from [devdocs.io/cpp](https://devdocs.io/cpp/). Set `ASKDOCS_ONLINE=0` to disable. No local doc corpus is bundled or required.

### Tutor commands (online docs)

| Command | Behavior |
|---------|----------|
| `/hint` | Socratic hint from online docs + cursor context |
| `/explain` | Concept explanation with doc links |
| `/debug` | Step-by-step debug guide |
| `/quiz` | Quiz from current topic |
| `/path loops` | Learning path from online search results |
| `/learn …` | Concept lesson from line + online docs + mermaid flow diagram |

Example:

```text
/learn teach me the concept used in line 83 in @app.cpp
```

Mention files in chat (`@app.cpp`) to load only that slice — not the whole repo. Tab completes `@` from the indexed workspace.

| Key | Action |
|-----|--------|
| `@` + name | Attach file context (autocomplete menu) |
| `Tab` / `Enter` | Accept `@` suggestion |
| `↑` / `↓` | Cycle `@` suggestions |

See [docs/NLP.md](docs/NLP.md) for architecture.

## Layout

```
┌ Files ────────┐┌ Editor ──────────────┐┌ AI Tutor ──────┐
│ ▸ build/      ││  1 #include ...      ││ tutor hints... │
│ ▾ src/        ││  2                   ││                │
│   main.cpp    ││  3 int main() {      ││ > /hint        │
└───────────────┘└──────────────────────┘└────────────────┘
 AskDocs EDITOR | main.cpp | c-family | AI:LEARN | Tab focus ...
```

## Controls

| Key | Action |
|-----|--------|
| `Tab` | Cycle focus: Files → Editor → Chat |
| `Ctrl-D` | Quit |
| `Ctrl-S` | Save current file |

### File explorer (Files pane)

| Key | Action |
|-----|--------|
| `↑` / `↓` | Move selection |
| `Enter` / `→` | Expand folder or open file |
| `←` | Collapse folder (or go up if on file) |
| `-` / `Backspace` | Go to parent directory |
| `a` | Toggle hidden files |
| `r` | Refresh directory |
| `PgUp` / `PgDn` | Scroll file list |

### Editor & chat

| Key | Action |
|-----|--------|
| Arrow keys | Move cursor (editor) |
| `/hint` `/explain` `/debug` `/quiz` `/step` `/trace` `/override` | Tutor commands |

## Syntax highlighting

Extension-based highlighting for common languages:

- **C-family:** `.cpp`, `.c`, `.h`, `.java`, `.cs`, `.swift`, …
- **Python, JavaScript/TypeScript, Rust, Go, Shell**
- **Markdown, JSON, YAML, HTML, CSS, SQL**
- **Generic fallback** for any other extension (comments, strings, numbers, common keywords)

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full system design.

## Roadmap

1. Piece-table buffer + mmap for large files
2. Operation-based undo/redo
3. Async Ollama client + constraint engine
4. LSP plugin slot, learning analytics
