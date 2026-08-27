# Token efficiency of programming languages and frameworks

**What it costs an LLM coding agent to work in each language and framework in the
[2025 Stack Overflow Developer Survey technology section](https://survey.stackoverflow.co/2025/technology).**

Measured on 918 large real-world source files from 92 open-source repositories
(67.2 MB, 1.86 M lines, 16.3 M tokens), plus 5,368 same-task implementations from
Rosetta Code used to normalise for functionality.

---

## Why two measurements are needed

"Tokens per line" is the number everyone reaches for, and it is close to useless as a
measure of efficiency: a language with short lines needs more of them. Across the 48
ranked languages, tokens-per-code-line and tokens-per-unit-of-functionality are
**negatively** correlated (Spearman ρ = −0.18). What actually predicts cost is how many
lines a language needs to express something (ρ = 0.94 against lines-per-task).

So the ranking below is built on functionality-normalised cost, with the raw
corpus density reported next to it.

### Track A — real-world corpus (the "same coding activities" measurement)

Per language/framework: 10 large files (6 KB–200 KB, ≥120 lines, median ≈ 2,000 lines)
taken from the biggest non-test, non-generated files of a flagship open-source project,
capped at 3 files per directory so one module cannot dominate. For frameworks the sample
is *application code written with* the framework (Django → Saleor, Laravel → Monica,
React → Excalidraw, Spring Boot → mall), not the framework's own source, except where
the framework is itself the library developers read (NumPy, jQuery, Node.js core).

Every file is then put through the **same standard edit session** — one realistic unit of
agent work on a 300-line slice of the file:

| Activity | Count | What it represents |
|---|---:|---|
| Full read of the slice | 1× | agent opens the file |
| 120-line focused re-read | 2× | agent re-reads around a symbol |
| 60-line search digest | 1× | grep/outline results |
| 12-line unified-diff hunk (3 lines changed) | 2× | the edits it emits |
| 40-line span of new code | 1× | a function it writes |

That total is the **tokens per edit session** column. Because the slice is a fixed 300
lines for every language, the column is directly comparable.

### Track B — functionality normalisation (Rosetta Code)

Track A cannot tell you whether a language needed more tokens *to do more*. Track B fixes
that: 140 Rosetta Code tasks implemented in ≥ 36 of the 48 languages (the median-sized
solution per task is used, to avoid rewarding code golf). Each language's cost for a task
is expressed as a ratio to the median cost of that task across all languages, and the
ratios are combined with a geometric mean. This is a fixed-effects style index, so a
language is never penalised for missing hard tasks.

**Tokens per equivalent task** = that index × the corpus-wide median task cost
(140.6 tokens). **Efficiency index** = 100 × median ÷ the language's own cost, so 100 is
the median language and higher is cheaper.

### Tokenizer

`o200k_base` (the GPT-4o/o200k BPE), run locally via `gpt-tokenizer`. Claude's tokenizer
is not public, so this is a proxy. It matters less than you would expect: re-running
everything on `cl100k_base` reproduces the language ranking at ρ = 0.999, and how
tokenizer-friendly a language is (chars per token) has essentially **zero** correlation
with its ranking (ρ = −0.002). Absolute token counts will shift a few percent on another
BPE; the ordering will not.

---

## Results

### Table 1 — Programming languages, ranked by token efficiency

| # | Language | Efficiency index | Tokens per equivalent task | Lines per task | Tokens per code line | Chars per token | Tokens per edit session |
|---:|---|---:|---:|---:|---:|---:|---:|
| 1 | MATLAB | 153 | 94.4 | 12.9 | 12.12 | 3.48 | 6,568 |
| 2 | Groovy | 151 | 96.1 | 10.7 | 10.08 | 4.79 | 5,981 |
| 3 | Crystal | 149 | 97.5 | 13.4 | 7.86 | 4.04 | 4,658 |
| 4 | Julia | 149 | 97.5 | 11.5 | 10.84 | 3.56 | 6,868 |
| 5 | Clojure | 147 | 98.6 | 11 | 12.99 | 4.42 | 8,014 |
| 6 | Ruby | 141 | 102.7 | 12.6 | 9.46 | 4.2 | 7,260 |
| 7 | Perl | 135 | 107.4 | 13.4 | 12.74 | 3.54 | 5,996 |
| 8 | R | 134 | 108.4 | 12.7 | 12.94 | 3.54 | 8,542 |
| 9 | Nim | 128 | 113.1 | 15.2 | 11.65 | 3.45 | 6,875 |
| 10 | Lisp | 123 | 117.8 | 12.1 | 12.15 | 3.99 | 7,645 |
| 11 | F# | 123 | 117.9 | 11.3 | 12.41 | 4.34 | 7,186 |
| 12 | Scala | 120 | 120.2 | 12.1 | 12.12 | 4.01 | 7,593 |
| 13 | Swift | 120 | 120.2 | 17.2 | 8.65 | 4.61 | 5,727 |
| 14 | Haskell | 117 | 123.3 | 14.6 | 11 | 4.08 | 7,155 |
| 15 | PowerShell | 116 | 125.3 | 14.7 | 8.99 | 4.88 | 5,750 |
| 16 | Lua | 115 | 126.4 | 16.5 | 9.77 | 3.79 | 6,182 |
| 17 | PHP | 111 | 130.3 | 17.2 | 10.51 | 4.64 | 5,830 |
| 18 | Tcl | 109 | 132.4 | 15.3 | 8.72 | 3.62 | 5,360 |
| 19 | MicroPython | 108 | 133.7 | 16.6 | 9.77 | 4 | 5,618 |
| 20 | Python | 108 | 133.7 | 16.6 | 8.08 | 5.14 | 5,281 |
| 21 | Apex | 102 | 141.8 | 17.6 | 9.1 | 4.35 | 5,569 |
| 22 | SQL | 102 | 142.2 | 19.9 | 11.49 | 3.61 | 6,905 |
| 23 | GDScript | 101 | 143.2 | 22.1 | 11.43 | 3.82 | 6,443 |
| 24 | Bash/Shell | 100 | 144.6 | 18.8 | 11.62 | 3.46 | 7,220 |
| 25 | Dart | 100 | 145 | 20.4 | 8 | 4.55 | 5,095 |
| 26 | Elixir | 100 | 145.1 | 16.1 | 11.36 | 3.54 | 6,250 |
| 27 | OCaml | 99 | 145.6 | 16.9 | 9.88 | 3.84 | 6,139 |
| 28 | Gleam | 95 | 151.9 | 21.9 | 7.54 | 4.14 | 4,678 |
| 29 | Erlang | 95 | 152.2 | 17.1 | 11.32 | 3.91 | 6,660 |
| 30 | TypeScript | 92 | 156.6 | 19.4 | 7.62 | 4.47 | 5,258 |
| 31 | Kotlin | 92 | 157.5 | 19.1 | 8.06 | 4.71 | 5,107 |
| 32 | JavaScript | 92 | 157.7 | 20.8 | 6.99 | 4.43 | 4,868 |
| 33 | Prolog | 92 | 158 | 17.8 | 9.76 | 3.52 | 6,008 |
| 34 | Rust | 90 | 161.5 | 21.8 | 8.27 | 4.59 | 5,368 |
| 35 | Java | 83 | 173.8 | 23.2 | 8.94 | 4.2 | 6,209 |
| 36 | C# | 82 | 177.5 | 27.2 | 7.59 | 5.48 | 4,828 |
| 37 | Objective-C | 78 | 185.1 | 25.8 | 10.12 | 4.48 | 6,757 |
| 38 | Visual Basic (.Net) | 75 | 192.1 | 24.4 | 11.23 | 5.63 | 6,150 |
| 39 | Delphi | 74 | 195.6 | 33 | 9.15 | 3.58 | 4,730 |
| 40 | VBA | 74 | 195.6 | 23.7 | 12.63 | 3.81 | 6,827 |
| 41 | Go | 73 | 198.4 | 31.8 | 9.48 | 3.8 | 5,962 |
| 42 | C++ | 70 | 207.9 | 29.7 | 9.83 | 4.05 | 6,027 |
| 43 | Ada | 67 | 217.6 | 26.9 | 8.92 | 4.48 | 5,200 |
| 44 | C | 64 | 225.5 | 32.2 | 8.54 | 3.65 | 5,317 |
| 45 | Zig | 57 | 253.3 | 28.7 | 10.29 | 3.35 | 6,842 |
| 46 | Fortran | 53 | 271.4 | 32.6 | 12.12 | 3.45 | 6,932 |
| 47 | Cobol | 41 | 356.7 | 46.7 | 11.97 | 5.76 | – |
| 48 | Assembly | 27 | 532.2 | 68.6 | 14.58 | 2.44 | 8,719 |
| – | HTML/CSS | – | – | – | 11.92 | 4.6 | 5,617 |
| – | Solidity | – | – | – | 10 | 4.15 | 5,662 |

### Table 2 — Frameworks, ranked by token efficiency

| # | Framework | Efficiency index | Tokens per code line | Host language | vs. host language | Median tokens per file | Tokens per edit session |
|---:|---|---:|---:|---|---:|---:|---:|
| 1 | Flutter | 128 | 6.88 | Dart | 0.86× | 4,705 | 4,216 |
| 2 | Gatsby | 128 | 6.88 | JavaScript | 0.98× | 4,751 | 4,553 |
| 3 | NestJS | 126 | 7.02 | TypeScript | 0.92× | 5,297 | 4,305 |
| 4 | React | 125 | 7.05 | TypeScript | 0.93× | 5,527.5 | 4,461 |
| 5 | Next.js | 119 | 7.44 | TypeScript | 0.98× | 11,413.5 | 4,867 |
| 6 | Vue.js | 116 | 7.62 | TypeScript | 1× | 2,151 | 4,272 |
| 7 | Elm | 114 | 7.74 | – | – | 7,092 | 4,343 |
| 8 | Django | 113 | 7.81 | Python | 0.97× | 17,214 | 4,834 |
| 9 | Deno | 113 | 7.84 | TypeScript | 1.03× | 10,671.5 | 4,645 |
| 10 | Node.js | 112 | 7.85 | JavaScript | 1.12× | 23,948 | 5,076 |
| 11 | ASP.NET CORE | 111 | 7.94 | C# | 1.05× | 11,620 | 5,166 |
| 12 | React Native | 110 | 8.01 | JavaScript | 1.15× | 7,395 | 5,574 |
| 13 | Strapi | 107 | 8.25 | TypeScript | 1.08× | 6,940.5 | 5,001 |
| 14 | FastAPI | 107 | 8.27 | Python | 1.02× | 9,735 | 4,932 |
| 15 | Fastify | 106 | 8.31 | JavaScript | 1.19× | 9,891.5 | 4,696 |
| 16 | Flask | 105 | 8.43 | Python | 1.04× | 20,476 | 5,295 |
| 17 | Solid.js | 104 | 8.49 | TypeScript | 1.11× | 3,870 | 5,355 |
| 18 | AngularJS | 103 | 8.58 | JavaScript | 1.23× | 15,277.5 | 5,802 |
| 19 | Express | 102 | 8.61 | JavaScript | 1.23× | 15,411 | 5,500 |
| 20 | Ruby on Rails | 102 | 8.65 | Ruby | 0.91× | 10,585 | 5,034 |
| 21 | htmx | 101 | 8.72 | JavaScript | 1.25× | 42,003 | 5,083 |
| 22 | Qwik | 100 | 8.82 | TypeScript | 1.16× | 10,307 | 5,704 |
| 23 | Remix | 99 | 8.91 | TypeScript | 1.17× | 10,574.5 | 4,906 |
| 24 | Blazor | 99 | 8.95 | C# | 1.18× | 9,933 | 5,947 |
| 25 | CodeIgniter | 98 | 8.99 | PHP | 0.86× | 11,205.5 | 4,839 |
| 26 | Angular | 98 | 9 | TypeScript | 1.18× | 8,302.5 | 5,584 |
| 27 | Spring Boot | 97 | 9.12 | Java | 1.02× | 3,067 | 4,912 |
| 28 | Astro | 97 | 9.13 | TypeScript | 1.2× | 8,464 | 5,663 |
| 29 | Phoenix | 96 | 9.18 | Elixir | 0.81× | 5,944.5 | 5,379 |
| 30 | Pandas | 96 | 9.19 | Python | 1.14× | 33,183 | 5,191 |
| 31 | Svelte | 96 | 9.22 | TypeScript | 1.21× | 5,018.5 | 5,353 |
| 32 | Yii 2 | 95 | 9.24 | PHP | 0.88× | 14,624 | 6,839 |
| 33 | jQuery | 95 | 9.33 | JavaScript | 1.33× | 6,801.5 | 5,395 |
| 34 | Laravel | 95 | 9.33 | PHP | 0.89× | 3,216 | 5,645 |
| 35 | Symfony | 90 | 9.83 | PHP | 0.94× | 10,509 | 6,458 |
| 36 | Scikit-Learn | 87 | 10.18 | Python | 1.26× | 26,289.5 | 5,613 |
| 37 | Drupal | 86 | 10.29 | PHP | 0.98× | 16,535.5 | 6,751 |
| 38 | NumPy | 85 | 10.38 | Python | 1.28× | 28,394.5 | 5,990 |
| 39 | PyTorch | 84 | 10.48 | Python | 1.3× | 26,922 | 6,472 |
| 40 | WordPress | 83 | 10.64 | PHP | 1.01× | 31,142 | 5,860 |
| 41 | Nuxt.js | 80 | 10.98 | TypeScript | 1.44× | 7,727 | 6,542 |
| 42 | TensorFlow | 80 | 11.09 | Python | 1.37× | 31,678.5 | 6,278 |
| 43 | Play Framework | 78 | 11.24 | Scala | 0.93× | 7,878.5 | 5,829 |

---

## What the numbers say

**1. Expressiveness dominates; tokenizer fit is noise.** Tokens per equivalent task tracks
lines per task at ρ = 0.94 and chars-per-token at ρ = −0.002. A language is cheap for an
agent because it needs fewer lines, not because the BPE likes its punctuation. Assembly
(2.44 chars/token) and Python (5.14) sit at opposite ends of tokenizer friendliness and
that fact predicts nothing about either one's ranking.

**2. The spread inside mainstream languages is about 1.7×.** For the same functionality:
Ruby 0.77×, Python 1.00×, TypeScript 1.17×, Rust 1.21×, Java 1.30×, C# 1.33×, Go 1.48×,
C++ 1.55×, C 1.69×. Go's cost is not verbosity of its lines — Go's lines are ordinary
(9.5 tokens each) — it is that Go needs 31.8 lines where Python needs 16.6.

**3. Per-line density is a trap.** JavaScript has the cheapest code lines in the survey
(6.99 tokens) and ranks 32nd. MATLAB's are among the priciest (12.12) and it ranks 1st.
Optimising an agent's context by preferring "short-line" languages optimises the wrong
quantity.

**4. Comments are a quarter of the bill.** Across the language corpus the median share of
file tokens sitting in comment lines is 23%. That is the single largest lever a team
controls without changing language — bigger than the gap between most adjacent ranks.

**5. Framework choice is mostly language choice.** The entire framework table spans
6.88–11.24 tokens per code line — a 1.6× range, against 2.1× across languages — and most
of it is inherited: every TypeScript framework lands in 7.0–11.0, every PHP one in
9.0–10.6, every Python one in 7.8–11.1. The `vs. host language` column isolates the
framework's own contribution and it rarely exceeds ±25%: Phoenix 0.81× Elixir and
Flutter 0.86× Dart at one end, Nuxt 1.44× TypeScript, TensorFlow 1.37× Python and
jQuery 1.33× JavaScript at the other. If you are picking a stack to minimise agent
token spend, the language decides it; the framework is a rounding error next to that.

**6. Legacy stacks are expensive twice over.** COBOL (2.5×) and Assembly (3.8×) cost the
most per unit of functionality *and* have the largest files, so an agent burns context
faster and has less room for reasoning. VBA, Visual Basic .NET and Delphi cluster at
1.37–1.39×.

## Robustness

| Check | Result |
|---|---|
| `o200k_base` vs `cl100k_base` ranking | ρ = 0.999 |
| Rosetta solution pick: median vs largest | ρ = 0.915 |
| Rosetta solution pick: median vs smallest | ρ = 0.892 |
| Rosetta solution pick: median vs first alphabetically | ρ = 0.965 |
| Tokens/task vs lines/task | ρ = 0.941 |
| Tokens/task vs tokens/code-line | ρ = −0.178 |

## Limits worth knowing before you quote these numbers

- **The framework table is not functionality-normalised.** No equivalent-task corpus
  exists across frameworks, so Table 2 measures the token density of code written in each
  framework, not how much code the framework saves you writing. A framework that halves
  the number of files you need would look unremarkable here. Read it as "what it costs to
  read a line of this stack", and use the `vs. host language` column for comparisons.
- **One repository per entry** (two for a few). Repo house style — line width, comment
  culture, formatter settings — moves the Track A columns by more than the gap between
  neighbouring ranks. The Rosetta axis, which is what the ranking uses, does not have this
  problem.
- **Rosetta Code is not production code.** Tasks are small and algorithmic; the corpus
  under-represents the framework glue, error handling and I/O that dominates real work.
  It is used only for *relative* cost between languages on identical work.
- **HTML/CSS and Solidity are unranked** — neither has a usable Rosetta Code task set, so
  only their corpus density is reported. **MicroPython** shares Python's Rosetta corpus
  (they are the same language on this axis); their Track A rows are genuinely different
  corpora and differ accordingly.
- **Small samples**: COBOL, R and SQL yielded only 6 qualifying large files each.
  COBOL has no `tokens per edit session` figure because none of its files reached 300
  lines.
- **Bundled third-party libraries are excluded** where a project vendors them into its own
  tree under a non-obvious path (WordPress's getID3, PHPMailer and sodium_compat copies
  were 6 of its 10 largest files); the exclusion is recorded per entry in the manifest.
- **Data files are excluded**, uniformly and mechanically: a file is dropped if >60% of
  its lines are <35% letters, or >10% of its characters are non-ASCII, or (non-markup
  only) it exceeds 20 tokens per code line. This removes locale tables, coefficient
  blocks, generated arity boilerplate and inline SVG data, which otherwise distort
  per-line figures by 2–5×.
- The Stack Overflow survey page itself is not reachable from this sandbox (blocked by the
  network egress policy), so the roster of languages and frameworks was reconstructed from
  the survey's published technology categories. A handful of low-share entries are absent.

## Reproducing

```
scripts/fetch_corpus.sh    data/*.tsv          # clone repos, sample 10 large files each
scripts/measure_corpus.js  <corpus> ...        # per-file token metrics + edit session
scripts/measure_rosetta.js /path/to/RosettaCodeData/Lang
scripts/aggregate.js                           # summary tables + correlations
scripts/render_tables.js                       # markdown
```

Raw per-file measurements: `data/corpus_files.csv` (918 rows).
Per-implementation Rosetta measurements: `data/rosetta_matrix.csv` (5,368 rows).
