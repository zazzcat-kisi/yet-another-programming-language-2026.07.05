# Plan — per-construct token cost analysis

> **Superseded in scope by `PLAN-llm-syntax.md`.** The construct inventory and mining method below
> survive as Phase 1 of that plan, cut to ~20 constructs and 10 donor languages.

**Goal.** For each common language construct, establish what it costs an LLM to read, write and
edit that construct in each language — and publish one table per construct, with the constructs
grouped into rough-cost bands.

This extends the existing survey (`REPORT.md`), which answered "what does a language cost per unit
of functionality". This one answers "*where inside the language does that cost come from*".

Status: **plan only.** No measurement has been run.

---

## 1. What "cost" means here

Four separable costs, measured separately and never blended into one number without showing the
parts:

| Cost | Definition | How it is obtained |
|---|---|---|
| **Syntax overhead** | Tokens the language charges to express the construct, excluding the payload it operates on | Mined from real code (§3.1) |
| **Task cost** | Tokens for a complete idiomatic instance doing one fixed job | Kata corpus (§3.2) |
| **Share of the bill** | Syntax overhead × how often the construct actually appears | Mined frequency × overhead (§3.3) |
| **Non-token friction** | Context dependence, edit blast radius, known failure modes | Scored rubric (§3.4) — judgment, labelled as such |

The headline per-construct number is **syntax overhead**, because it is the part the language
controls and the part that repeats thousands of times in a codebase. Task cost is the control for
constructs whose payload can't be separated cleanly.

### The key measurement idea

For most constructs the token cost splits cleanly into *scaffolding* and *payload*:

```
tokens(construct node) − tokens(payload children) = syntax overhead
```

A `for` loop's payload is its body; a function's payload is its body; a `try/catch`'s payload is the
guarded block and the handler body. Subtracting the payload removes "what the code happened to be
doing" and leaves what the language charged to say it. Measured over every instance in the corpus,
reported as **median + interquartile range**, not a single number — the spread is itself a finding.

For constructs where that split is meaningless (optional chaining, ternary, string interpolation,
type annotations), the kata corpus carries the measurement instead.

---

## 2. Scope

### 2.1 Languages — set by grammar availability (audited)

| Tier | Coverage | Languages |
|---|---|---|
| **1** | Full construct set, mined + kata | Python, JavaScript, TypeScript, Java, C, C++, C#, Go, Rust, Ruby, PHP, Swift, Kotlin, Scala, Bash, Lua, Haskell, OCaml, Elixir, Julia, Dart, Clojure, PowerShell, Perl, MicroPython |
| **2** | Core 20 constructs, mined where the grammar holds up, else kata | Zig, Gleam, Groovy, Solidity, Objective-C, MATLAB, Prolog, GDScript, Lisp, VBA, COBOL, Delphi/Pascal, HTML/CSS |
| **3** | Kata only, or excluded | Fortran, Nim, Tcl, Ada, Crystal, R, Erlang, Apex, Visual Basic .NET, Assembly, SQL |

Tier 3 exists because npm has no usable tree-sitter grammar for those languages (`tree-sitter-r` and
`tree-sitter-erlang` on npm are placeholder security stubs; Fortran, Nim, Tcl, Ada, Crystal have
none). Grammars for several do exist as source repos and can be built with the tree-sitter CLI —
that is a costed option in Phase 0, not an assumption.

Assembly, SQL and HTML/CSS lack most of these constructs by nature and are reported as N/A rather
than forced into the tables.

### 2.2 Corpus

Reuse the existing Track A corpus: 918 large real-world files, 92 repositories, 1.86 M lines. No
refetch. Where a Tier-1 language yields fewer than 30 instances of a construct, top up from the same
repository before falling back to the kata number.

---

## 3. Methodology

### 3.0 Phase 0 — grammar and query bring-up
Install grammars, confirm each parses its slice of the corpus with an acceptable error-node rate
(target: <2% of files containing an ERROR node), and decide from measured results which Tier-2
languages hold up. Languages whose grammar can't parse their own corpus drop to kata-only. This
phase also decides whether building the missing grammars from source is worth it.

### 3.1 Mined syntax overhead (primary)
- One tree-sitter query per (construct, language), matching the construct node and tagging its
  payload children.
- For every match: `overhead = tokens(node) − tokens(payload)`, tokenised with `o200k_base` via the
  existing harness.
- Report median, IQR, and n. Constructs with n < 30 are marked *thin* and fall back to kata.
- **Query precision is validated, not assumed**: for each query, 20 random matches are inspected by
  hand and a precision figure is recorded per query in the published data. A query below 90%
  precision is rewritten or dropped.

### 3.2 Kata corpus (control, and primary for un-mineable constructs)
- Each construct gets one written **specification**: exactly what the snippet must do, with fixed
  identifier names (`items`, `total`, `key`, `value`) and fixed arity, so identifier-length
  differences don't leak into the comparison.
- One idiomatic implementation per language — what a competent developer writes, not golf, not
  padded.
- Two numbers recorded: **inline cost** (the construct) and **wrapper cost** (mandatory enclosing
  scaffolding the language forces, e.g. Java's class-and-method, Go's `package main`). Wrapper cost
  is reported once per language, never charged per instance.
- **Validation ladder**, recorded per snippet: `executed` (runs and passes its assertion) >
  `parsed` (tree-sitter, zero error nodes) > `reviewed` (neither available). Node, Python, Ruby,
  PHP, Perl, Bash and Lua can realistically reach `executed` in this sandbox; most others stop at
  `parsed`, and that limit is stated in the output.

### 3.3 Frequency weighting
`instances per 1,000 SLOC` from the corpus × median overhead = **share of the token bill**. This is
what makes the analysis actionable: a construct that is expensive but rare matters less than a cheap
one written on every third line. Both orderings get a summary table.

### 3.4 Non-token friction (rubric — explicitly judgment)
Three 0–3 scores per (construct, language), each with written anchors so the scoring is repeatable:
- **Context dependence** — can the construct be understood from the line alone, or does it need type
  information, an implicit receiver, an operator overload, or a macro expansion resolved elsewhere?
- **Edit blast radius** — does changing one instance force edits elsewhere (adding an enum case and
  every exhaustive match, changing a signature and every caller)? Where the corpus can measure the
  ripple, the measured number replaces the score.
- **Failure-mode density** — documented, citable LLM error modes for that construct in that language
  (C-style loop off-by-one, dropped `err != nil`, Python mutable default arguments, JS `this`
  binding, Rust lifetime annotations). Drawn from a written list assembled up front, not invented
  row by row.

These are reported in their own columns and never folded into the token figure.

### 3.5 Generation reliability (optional stretch)
For ~10 constructs × ~12 languages, sample K=10 generations against the construct spec and score by
parse + assertion. Converts to "expected retries", and retries convert to tokens. Recommended
**out of scope for the first pass** — it needs model API budget and roughly doubles the work.

---

## 4. Construct list

44 constructs in 10 families. **★ = core 20** (the set every tier gets).

**Bindings and data**
1. ★ Local mutable variable declaration
2. ★ Constant / immutable binding
3. Explicit type annotation on a local
4. Destructuring / multiple assignment
5. ★ Array/list literal + index access
6. ★ Map/dictionary literal + key lookup
7. ★ String literal with interpolation
8. Multi-line / raw string literal

**Functions**
9. ★ Top-level function definition (2 params, returns a value)
10. ★ Anonymous function / lambda / closure
11. Default parameter value
12. Variadic parameters
13. Call with named / keyword arguments
14. Returning multiple values or a tuple

**Control flow**
15. ★ `if` / `else if` / `else` chain
16. ★ Counted loop over a numeric range
17. ★ Iterate over a collection
18. Iterate over a map with key and value
19. `while` loop
20. ★ Switch / match / pattern match over 3 cases
21. Early-return guard clause
22. `break` / `continue` under a condition
23. Ternary / conditional expression

**Types and abstraction**
24. ★ Struct / record / class with 3 fields
25. ★ Method on a type, with a field access
26. Object / instance construction
27. Enum or sum type with 3 variants
28. ★ Interface / protocol / trait declaration
29. Implementing an interface on a type
30. Generic function over one type parameter
31. Type alias

**Collections and functional**
32. ★ map / filter / reduce chain over a collection
33. Sort with a custom comparator
34. Comprehension (where the language has one)

**Nullability**
35. ★ Null / optional check before use
36. Optional chaining or safe navigation

**Errors and resources**
37. ★ Raise / throw an error
38. ★ Catch and handle an error
39. ★ Propagate an error to the caller (idiomatic form: `?`, `err != nil`, re-raise)
40. Scoped resource cleanup (`with`, `using`, `defer`, RAII)

**Modules and I/O**
41. ★ Import a module or symbol
42. Module / namespace declaration with visibility
43. ★ Print a formatted line to stdout
44. Read a file into a string

**Concurrency**
45. `async` function definition + await one call
46. Spawn a concurrent task
47. Channel send/receive or mutex-guarded update

**Documentation and tests**
48. ★ Doc comment for a function (language's canonical form)
49. Unit test with one assertion

That is 49 items; the list is deliberately over-inclusive and will be cut to ~44 in Phase 0 by
dropping any construct that fewer than 60% of Tier-1 languages express distinctly.

**Handling absence.** A construct a language lacks is not blank. It records the cost of the
*idiomatic workaround* and is flagged: C has no `match`, so it pays the `if`/`else` chain price;
Go had no generics until 1.18, so pre-generic interface indirection is the workaround. The
workaround cost is the honest answer to "what does this construct cost me here".

---

## 5. Output shape

Six sections, one per cost band, banded on the **cross-language median syntax overhead**:

| Band | Median overhead | Example of what lands here |
|---|---|---|
| 0 — Structural | ≤ 3 tokens | block delimiters, `else` |
| 1 — Trivial | 4–10 | variable declaration, index access |
| 2 — Cheap | 11–25 | collection iteration, guard clause |
| 3 — Moderate | 26–60 | function definition, error handling |
| 4 — Expensive | 61–150 | interface + implementation, generic function |
| 5 — Heavyweight | > 150 | class with constructor, test declaration |

Within each band, one table per construct:

| Language | Tokens (median) | IQR | n | vs. cross-language median | Wrapper | Freq /KLOC | Share of bill | Ctx | Ripple | Risk |

Each table is followed by one line naming what drives the spread, and the cheapest/dearest pair with
its ratio. Two summary tables lead the document: constructs ranked by median cost, and constructs
ranked by share of the real token bill — those two orderings will not match, and the gap between
them is the most useful thing the analysis produces.

---

## 6. Work plan

| Phase | Work | Output |
|---|---|---|
| 0 | Grammar bring-up; parse-rate check on the existing corpus; final tier and construct cuts | `data/grammar_audit.csv` |
| 1 | Write the 44 construct specifications with fixed identifiers and anchors | `data/constructs.yaml` |
| 2 | Author tree-sitter queries per construct × Tier-1 language; validate precision on 20 samples each | `queries/`, `data/query_precision.csv` |
| 3 | Mine the corpus; compute overhead median/IQR/n and frequency per KLOC | `data/construct_instances.csv` |
| 4 | Author and validate the kata corpus for Tier-2/3 and un-mineable constructs | `katas/`, `data/kata_costs.csv` |
| 5 | Score the rubric; assemble bands; render tables; write the findings | `REPORT-constructs.md` + artifact |

Phases 2 and 4 are the bulk of the effort. Phase 2 is roughly 44 × 25 queries, though most reduce to
a per-family template with a handful of per-language overrides.

---

## 7. Known risks

- **Query precision is the whole ballgame.** A `for`-loop query that silently misses Python's
  `enumerate` idiom or catches comprehensions will produce a confident wrong number. Hence the
  mandatory hand-validated precision figure per query, published alongside the results.
- **"Idiomatic" is contested.** Mining real code sidesteps most of this; where katas are used, the
  author's taste is a real bias and the snippet ships in the repo so it can be argued with.
- **Payload subtraction is imperfect** for constructs whose scaffolding interleaves with the payload
  (Lisp macros, Haskell where-clauses, Ruby blocks). Those constructs are flagged and lean on kata
  numbers.
- **Corpus is one repository per language.** House style affects frequency counts more than it
  affects per-instance overhead. Frequency figures carry a wider error bar than cost figures, and
  will be presented that way.
- **Tokenizer is a proxy** — `o200k_base`, as in the main survey, cross-checked on `cl100k_base`.
  Ranking was stable at ρ = 0.999 there; the same check gets re-run here rather than assumed.

---

## 8. Two calls I'd like from you

1. **Language scope.** Default is the 25 Tier-1 languages for the full construct set, with Tier 2
   getting the core 20. Going wider means building grammars from source (adds a phase); going
   narrower — say the 12 most-used languages — would cut Phase 2 by more than half.
2. **The reliability probe (§3.5).** Default is out of scope for the first pass. It is the only
   part that measures how often an LLM *gets the construct wrong*, which is arguably the cost that
   matters most — but it needs API budget and roughly doubles the timeline.
