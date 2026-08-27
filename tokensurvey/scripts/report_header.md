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

