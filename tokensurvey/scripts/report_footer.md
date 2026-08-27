
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
