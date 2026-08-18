# doc-conventions

Detailed conventions for the `update-docs` skill. Load this before touching any file.

## Directory shape

```
docs/
  index.html              landing page: what the fork is, the two platforms, build
                            system summary, links to every page
  style.css                 shared stylesheet — reuse it, don't recreate. Add rules if
                             a page genuinely needs something new; don't fork the file.
  integration.html          top-level (not under a subdir): how to use this SDK from
                             another buildcpp-based project
  platforms/<name>.html     one per --platform value (rp2040, host, ...)
  build/overview.html       how build.cpp works mechanically
  build/scope.html          the exclusion tables — what's left out and why
```

Unlike a project documenting its own hand-written classes, pico-sdk's docs track
**`build.cpp` and its decisions**, not the vendored SDK source under `src/` — that code
is untouched upstream, not ours to document class-by-class. A new doc page gets added
when `build.cpp` grows a new platform, or when scope/exclusions/CLI args change enough
to need a table update — not when vendored source changes.

## Self-containment (non-negotiable)

- No CDN links, no external fonts, no `<script src="https://...">`. Everything must
  work opened as a local file with no network access.
- Code blocks are plain `<pre><code>` styled by `style.css` alone — no JS syntax
  highlighter.
- Plain `<a href="https://...">` text links to real external references (e.g. the
  buildcpp repo, the ARM GNU Toolchain download page) are fine — they're inert until
  clicked. The rule is against anything the page *loads* automatically (a script, a
  stylesheet, a font, an image) from a remote host.
- If a page genuinely needs a diagram, prefer a hand-rolled inline `<svg>` over pulling
  in a charting library — not expected to come up often here (this SDK's docs are
  mostly tables and code blocks, not data-flow diagrams).
- After writing/editing pages: grep the touched files for `http://`/`https://` and
  confirm every match is a plain `<a href>` text link, never a `<script src>`/
  `<link rel="stylesheet">`/CDN reference.

## Nav block (copy into every page, including new ones)

Every page gets the identical nav, with `class="here"` moved to the current page's own
link. Relative paths: pages one level deep under `docs/` (e.g. `docs/platforms/x.html`,
`docs/build/x.html`) use `../` to reach `index.html`/`style.css` and the other
top-level pages; `index.html` and `integration.html` themselves (both directly under
`docs/`) use no prefix. When you add a new page, add its link to this block **in every
existing page**, not just the new one — grep for `<h4>platforms</h4>` (or the relevant
group) to find every copy that needs the new `<a>` line.

```html
<nav>
  <a class="brand" href="../index.html">pico<span>-sdk</span></a>
  <span class="tag">C11 · buildcpp · RP2040</span>

  <h4>platforms</h4>
  <a href="../platforms/rp2040.html">RP2040</a>
  <a href="../platforms/host.html">Host</a>

  <h4>build</h4>
  <a href="../build/overview.html">build.cpp</a>
  <a href="../build/scope.html">Scope &amp; exclusions</a>

  <h4>usage</h4>
  <a href="../integration.html">Integration guide</a>
</nav>
```
(Adjust `href` prefixes for `index.html`/`integration.html`, which have no `../` since
they're already at `docs/`'s top level. Add new `<h4>group</h4>` sections / `<a>`
entries as new platforms or build-doc topics appear. Mark the current page's own `<a>`
with `class="here"` instead of a plain `href`-only tag.)

## Per-page shape

### Platform pages (`platforms/<name>.html`)

```html
<h1>Platform: <code>name</code></h1>
<p class="lede">One sentence: what it's for, in the reader's terms.</p>

<h2>Role</h2>
<p>What this platform builds, and how you select it (--platform flag value).</p>

<h2>Compiler configuration</h2>
<pre><code>-- real flags, copied from the current build.cpp, not paraphrased --</code></pre>

<h2>Worth knowing</h2>
<ul>
  <li>Non-obvious facts — caveats, missing pieces, verified/unverified claims — that
      would otherwise live as a comment in build.cpp.</li>
</ul>

<footer>Source: <code>build.cpp</code> (name branch)</footer>
```

### Build pages (`build/overview.html`, `build/scope.html`)

Free-form prose + tables — `overview.html` walks through build.cpp's mechanics
step-by-step (numbered `<h3>` sections read well here), `scope.html` is primarily
tables (one row per excluded directory/file, with the reason pulled verbatim from
build.cpp's inline comment).

### `integration.html`

Numbered `<h2>` steps (add submodule → build it → wire into your own build.cpp →
verify), with a real, complete `Include<Direct>`/`Link<Path>` code example — not a
fragment. Keep it in sync with whatever buildcpp's actual API surface is; if buildcpp
adds new primitives, update the example to use them where appropriate rather than
leaving it stale.

Keep "Worth knowing" / caveat sections to real, specific facts (what's verified vs.
not, what's still missing, a gotcha that bit during development) — not generic filler.

## Comment-trim rule (applies to `build.cpp` only, never to vendored `src/`)

`src/` is upstream pico-sdk source, unmodified except for the deliberate exclusions
tracked in `build/scope.html`. Its comments are Raspberry Pi's, not ours to sweep,
rewrite, or delete — leave it alone entirely.

`build.cpp` is the one file we own. Delete any comment there that narrates *process*:
why an approach was chosen over an alternative, what bug it fixed, what upstream
CMake/Bazel used to do that this replaces. That content moves into the relevant doc
page instead (usually `build/overview.html`'s "Worth knowing" or the affected
platform's own page).

Keep a comment in `build.cpp` only if it's a **single line** flagging something a
reader would otherwise get wrong at that exact spot — the shell-quoting gotcha on
`-DPICO_BOARD`, why the compiler is `clang` not `clang++`, why per-platform build dirs
exist. If a multi-line comment contains exactly one fact worth keeping, trim it to one
line; never leave a multi-line comment in `build.cpp` once its rationale has a home in
`docs/`.

To check the sweep is complete, scan for any remaining run of 2+ consecutive `//`
comment lines in `build.cpp`:
```sh
awk '
  /^\s*\/\// { c++; if (c==1) start=NR; next }
  { if (c>=2) print FILENAME":"start"-"NR-1" ("c" lines)"; c=0 }
  END { if (c>=2) print FILENAME":"start"-"NR" ("c" lines)" }
' build.cpp
```

## Verification checklist

- `./build` and `./build --platform host` both succeed clean.
- If a comment sweep happened: `git diff build.cpp` shows comment-only changes, no
  behavior changed. If a sweep and a real code change happened in the same session,
  make sure they're not tangled in a way that makes this hard to confirm.
- Every `href` in every touched/new page resolves to a real file (a broken nav link is
  the most common mistake when a new page gets added but not wired into every nav
  copy) — a quick check:
  ```sh
  cd docs
  for f in index.html integration.html platforms/*.html build/*.html; do
    dir=$(dirname "$f")
    grep -oE 'href="[^"]*"' "$f" | sed 's/href="//;s/"$//' | grep -v '^http' | grep -v '^#' | sed 's/#.*//' | while read -r href; do
      target=$(python3 -c "import os; print(os.path.normpath(os.path.join('$dir', '$href')))")
      [ -f "$target" ] || echo "BROKEN in $f: $href"
    done
  done
  ```
- No `http://`/`https://` except plain `<a href>` text links — never a `<script src>`,
  `<link rel="stylesheet">`, or other auto-loaded remote resource.
