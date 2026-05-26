# NLP & Agent Layer

Docs come from **online sources** (cppreference.com, docs.python.org). Code context is loaded selectively — never the full repo.

## Pipeline

```
User message
    → IntentNet (BoW + 2-layer classifier)
    → search_online_docs() via curl
    → ContextAgent (LRU file buffer, max 4 files)
    → TutorService (mode + learning constraints)
    → Chat panel
```

## Intent classification

`IntentNet` maps a 48-dim bag-of-words vector to intents:

| Intent | Example triggers |
|--------|------------------|
| hint | help, stuck, next step |
| explain | why, how, understand |
| debug | error, bug, crash |
| quiz | test, edge case |
| path | learn, roadmap, start |
| navigate | find file, @main.cpp |

Commands (`/hint`, `/path`, `/learn`, …) override the classifier.

## Online doc search

The agent calls `search_online_docs(query, lang)` which uses `curl` to search:

- **C/C++** → [cppreference.com](https://en.cppreference.com)
- **Python** → [docs.python.org](https://docs.python.org)
- **Fallback** → DuckDuckGo site-restricted search

Disable with `ASKDOCS_ONLINE=0`. No local `.adoc` corpus is loaded.

## Context agent (not full-repo cache)

| Rule | Value |
|------|-------|
| Max files in buffer | 4 |
| Max lines per file slice | 80 |
| Workspace index | 8k files max, single path pool |
| Index structure | sorted basenames → offsets into path pool |

### @ mentions

Type `@` in chat to autocomplete filenames from the index. Tab or Enter inserts `@main.cpp`. On submit, tagged files are resolved and loaded into the context buffer only — never the full tree.

Loads:

1. Current editor file (window around cursor)
2. `@file.cpp` or `main.py` mentions in chat
3. `#include` / `import` links (one hop)

## Learning path

`/path loops` builds a reading list from top online doc hits for the topic.

## Agent trace + streaming

On submit the tutor streams in two phases:

1. **Agent steps** (`⚙`) — intent, online doc search, `@` resolve, slice load, compose
2. **Tutor response** — text streams in real time

Example:

```
⚙ ① classify intent → explain_line (83)
⚙ ② fetch line 83 from app.cpp
⚙ ③ extract symbols → cursor, row, +=
⚙ ④ search online docs → Compound assignment (cppreference.com)
  Line 83 in src/app.cpp:
  > 83 | editor_.cursor.row += 1;
  ...
```

### Line-aware answers

Questions like `what does line 83 do` trigger a dedicated agent path:

1. Parse line number from the question
2. Load that line (+ neighbors) from the open file or `@file.cpp`
3. Extract symbols/keywords from the line
4. Search online docs
5. Stream a plain-language explanation with links

Works with: `what does line 83 do`, `@app.cpp line 120`, `explain line 5`

## `/learn` — concept lessons

`/learn` teaches the **concept** behind a line. It combines keyword extraction, online doc search, and your `@` / editor context, plus a mermaid flowchart.

Example:

```text
/learn teach me the concept used in line 83 in @app.cpp
```

## Adding languages

Extend `language_from_extension` in `file_io.cpp` and add a search target in `online_search.cpp`.

## Future

- Train IntentNet weights from labeled chat logs
- Cache online search results per session
- Async Ollama layer with constraint envelope on top of this context pack
