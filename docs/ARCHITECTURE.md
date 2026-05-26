# AskDocs — System Architecture

A terminal-based, learning-first programming environment. AI acts as a Socratic tutor, not a code generator.

## High-Level Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           AskDocs Process                                │
├──────────────────────────────┬──────────────────────────────────────────┤
│      CORE EDITOR LAYER       │           AI LEARNING LAYER               │
│  ┌──────────┐ ┌───────────┐  │  ┌─────────────┐  ┌──────────────────┐  │
│  │  Buffer  │ │  Renderer │  │  │  Context    │  │ Constraint Engine │  │
│  │ (Piece   │ │ (ANSI inc.│  │  │  Compressor │──│ (pre-LLM rules)   │  │
│  │  Table)  │ │  redraw)  │  │  └──────┬──────┘  └────────┬─────────┘  │
│  └────┬─────┘ └─────┬─────┘  │         │                  │             │
│       │             │        │  ┌──────▼──────┐  ┌────────▼─────────┐  │
│  ┌────▼─────┐ ┌─────▼─────┐  │  │   Prompt    │  │  Response Parser  │  │
│  │ Undo/    │ │  Input    │  │  │   Builder   │  │  (mode-aware)     │  │
│  │ Redo     │ │  Handler  │  │  └──────┬──────┘  └────────┬─────────┘  │
│  └────┬─────┘ └─────┬─────┘  │         │                  │             │
│       │             │        │  ┌──────▼──────────────────▼─────────┐   │
│  ┌────▼─────────────▼─────┐  │  │     Async LLM Client (Ollama)     │   │
│  │    File System Layer    │  │  └───────────────────────────────────┘   │
│  │ (mmap, dirty, incr. w)  │  │                                          │
│  └─────────────────────────┘  │  ┌───────────────────────────────────┐   │
│                               │  │         Chat UI Panel              │   │
│                               │  │  history · commands · scroll       │   │
│                               │  └───────────────────────────────────┘   │
└──────────────────────────────┴──────────────────────────────────────────┘
         ▲                                    ▲
         │         Integration Layer          │
         └──────── event bus / focus ─────────┘
```

## Core Data Structure Choices

### Text Buffer: Piece Table (chosen over Rope)

| Criterion | Piece Table | Rope |
|-----------|-------------|------|
| Large file (>1GB) | Original buffer mmap'd; edits append-only | Tree overhead per node |
| Insert/delete | O(pieces) amortized; no copy of original | O(log n) but more pointers |
| Memory | Two buffers + piece list; cache-friendly scans | Pointer-chasing in tree |
| Undo | Operation log maps cleanly to piece ops | Same, but more complex |

**Decision:** Piece table with mmap'd original file + append buffer. Edits never copy the original file into RAM.

### Undo/Redo: Operation-Based Stack

Each entry stores `{type, pos, deleted_text | inserted_text}` — no buffer snapshots.

### Rendering: Dirty-Line Bitmap

Per viewport row, a bitset marks lines needing redraw. Full-screen clear only on terminal resize.

## Memory Model

```
Arena (per session)
├── Piece table metadata (fixed pool)
├── Undo stack nodes (ring buffer, pre-allocated)
├── Render scratch (reused UTF-8 line buffer)
└── Chat history (capped deque, ~200 messages)

mmap region
└── Original file (read-only, OS page cache)
```

- **Typing hot path:** zero heap allocations; mutate piece list in-place; mark 1–3 dirty lines.
- **Chat path:** async worker thread; UI receives completed messages via lock-free queue.

## AI Constraint System

Rules applied **before** any HTTP request leaves the process:

```
User input → CommandParser → ModeResolver → ConstraintEngine → PromptBuilder → LLM
                                                    │
                                    ┌───────────────┼───────────────┐
                                    ▼               ▼               ▼
                              Block full      Require user    Inject system
                              solutions       attempt first   tutor persona
                              (Learn mode)    (Learn/Debug)   (all modes)
```

### Modes

| Mode | Default behavior | Code output |
|------|------------------|-------------|
| LEARN | hints, questions | forbidden |
| DEBUG | error analysis, step fixes | snippets only |
| EXPLAIN | mental models | no replacement code |
| QUIZ | generates questions | none |
| OVERRIDE | full solution | allowed (explicit `/override`) |

### Prompt envelope (always prepended)

```
You are a Socratic tutor. Never give full solutions unless OVERRIDE mode.
Ask one clarifying question before answering when context is ambiguous.
Prefer hints over code. Explain reasoning before any code snippet.
```

## Build Phases

| Phase | Scope | Status |
|-------|-------|--------|
| 1 | Core buffer (piece table, search) | planned |
| 2 | Rendering + input | **UI prototype (this repo)** |
| 3 | File system (mmap, dirty) | planned |
| 4 | Undo/redo | planned |
| 5 | Chat UI | **UI prototype (this repo)** |
| 6 | AI integration (Ollama async) | planned |
| 7 | Constraint engine | planned |

## Future Roadmap

- **LSP:** optional plugin slot; keep core editor LLM-free
- **Learning analytics:** track hint→attempt→success ratios locally
- **Plugin API:** minimal C ABI for language runners and formatters
- **Session export:** anonymized study session replay for self-review
