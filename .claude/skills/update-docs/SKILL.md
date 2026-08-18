---
name: update-docs
description: Updates pico-sdk's self-contained HTML docs (docs/) to match the current
  state of build.cpp — the platform pages, the scope/exclusions list, and the
  integration guide. Use when the user asks to "update the docs", "refresh the
  documentation", "sync docs with build.cpp", "the docs are out of date", "document the
  new platform/component", or after build.cpp changes (a new --platform, a new
  exclusion, a new CLI arg) and the docs need to catch up.
allowed-tools: Read, Edit, Write, Grep, Glob, Bash
---

# Update docs

`docs/` documents `build.cpp` — the buildcpp port itself (platforms, scope, CLI args,
integration) — not the vendored SDK source under `src/`, which is untouched upstream
code we don't own and don't comment-sweep. See `references/doc-conventions.md` for the
exact template (nav block, style.css, page shape, comment-trim rule, self-containment)
before writing anything.

1. **Find what changed.** `git diff <since>...HEAD -- build.cpp` (or `git log --stat --
   build.cpp` since `docs/` was last touched) — `build.cpp` is the only source file
   these docs track. A change to vendored `src/` code is not, on its own, a reason to
   touch docs.
2. **What kind of change, what page it hits:**
   - New/changed `--platform` or `--link` value → update `build/overview.html`'s CLI
     args table, and if it's a new platform, a new `docs/platforms/<name>.html` page
     following the shape in `references/doc-conventions.md`.
   - New/changed entry in `kRp2040ExcludedDirs`/`kRp2040ExcludedFiles`/
     `kHostExcludedFiles` (or an equivalent for a new platform) → update
     `build/scope.html`'s tables. Every exclusion needs the same one-line "why" that's
     already a comment next to it in `build.cpp` — pull the reason from there, don't
     invent one.
   - Change to compile/link flags for an existing platform → update that platform's
     "Compiler configuration" `<pre>` block in `docs/platforms/<name>.html` to match
     `build.cpp` exactly.
   - New buildcpp-level fix or gotcha discovered while working on `build.cpp` → a line
     in `build/overview.html`'s "Worth knowing" list, not scattered across other pages.
3. **New page → wire it into every nav block**, including `docs/index.html`'s own
   platform/build boxes. `references/doc-conventions.md` has the nav template to copy.
4. **Sweep comments in `build.cpp` only** (never in `src/` — that's vendored upstream
   code, not ours to rewrite) using the same rule as GameEngine's version of this
   skill: multi-line rationale comments move into the doc page and get deleted from
   `build.cpp`; keep only single-line notes flagging something genuinely non-obvious at
   that exact spot.
5. **Verify.** `./build` and `./build --platform host` both still succeed clean,
   `git status` shows `build.cpp` changed only in comments (no behavior changed) if a
   sweep happened, every new/changed doc link resolves, no external network references
   introduced beyond inert `<a href>` text links (self-contained otherwise — see
   `references/doc-conventions.md`).
6. **Don't commit.** Leave changes staged/unstaged for the user to review, same as
   every other change in this repo.
