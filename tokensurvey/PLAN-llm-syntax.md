# Plan — what syntax should a language have if only LLMs read and write it

**End goal (from you).** A programming language whose canonical file form is binary, read and
written only by LLMs and tools. Humans understand a program by asking a tool to project it into
something human-readable (an IDE "text view"). This phase answers one question: **what should the
form the model actually reads and writes look like?**

Supersedes the scope decisions in `PLAN-constructs.md`. That document's construct inventory and
mining method survive as Phase 1 here, at reduced scope.

---

## 1. The reframe

The original plan measured what constructs cost in existing languages. That was the right instinct
and the wrong target, for three reasons.

**We control the encoding, so most existing-language token costs don't transfer.** That `function`
costs more tokens than `fn` in JavaScript is not a finding about JavaScript — it's a finding about a
choice we get to make freely. Measuring 25 languages' keyword spellings tells us almost nothing we
can't decide by fiat once we know the tokenizer.

**What does transfer is structural.** Whether a model can close a block correctly 1,200 lines later.
Whether it can edit a fragment without seeing the file. Whether it silently drops an error check.
Whether redundancy helps it self-correct or just costs tokens. These are properties of *shape*, and
they carry across to a new language.

**Everything a model is "good at" in Python is confounded with having seen enormous amounts of
Python.** This is the threat that would invalidate the whole study if ignored. A brand-new language
starts with none of that prior. So the study's central instrument has to separate *structure* from
*familiarity* — otherwise we would confidently design a language out of properties that were really
just "the model has read a lot of this".

### What we are actually designing

Because the canonical file is binary, the model never sees it. It sees a **projection**, and it
edits through a **protocol**. So the deliverable is not one syntax — it is:

1. a **read projection** (what the model is shown),
2. an **edit protocol** (how it expresses a change), and
3. optionally a **write projection** if the cheapest thing to write differs from the cheapest thing
   to read — which is likely, and which no human-facing language can do because humans need one
   consistent surface.

That asymmetry is the most interesting freedom the binary decision buys, and it should be tested
explicitly rather than assumed away.

### One thing worth saying plainly before we spend anything

**Binary files do not save tokens.** The model reads text either way; the encoding underneath is
invisible to it. What binary buys is that tools *must* mediate — which lets the store carry types,
stable node IDs, and provenance, and ends formatting debates. The token savings live one level up,
in **what the projection chooses to show**. Showing a 60-line slice plus the signatures it
references, instead of a 2,000-line file, is a multiple; picking better keywords is tens of percent.
So this plan spends a slice of its budget on projection policy, not only on syntax.

---

## 2. Explaining the scope question I asked badly

What I was asking was narrow and mechanical: **how many languages do we hand-build measurement
machinery for?** Every language in the study needs its own tree-sitter query per construct (and,
where mining fails, a hand-written snippet). At 44 constructs that's ~1,100 authored cells for 25
languages versus ~530 for 12. More languages = a broader base for claims about "LLMs like X"; fewer
= the same study, sooner, on shakier generalisation.

**Given the end goal, that question is the wrong one.** We don't need coverage of what's *popular* —
nobody is going to write our language in Kotlin's style because Kotlin is popular. We need coverage
of *distinct syntactic strategies*, so that every design axis has at least two real languages
sitting at opposite ends of it. That's a much smaller set.

**Donor languages (10), chosen for strategy coverage:**

| Language | Represents |
|---|---|
| Python | Indentation-delimited, dynamic, one-obvious-way |
| TypeScript | Braces, gradual/structural typing, huge prior |
| Java | Explicit-verbose extreme, nominal typing, exceptions |
| Go | Minimal feature set, explicit multi-return errors, enforced formatting |
| Rust | Dense type syntax, `Result` + `?`, lifetime annotations |
| C | Braces, no abstraction, manual everything |
| Ruby | `end` keywords, blocks, many ways to say the same thing |
| Clojure | S-expressions, uniform, homoiconic — structure *is* the syntax |
| Haskell | Inference-heavy, dense, point-free |
| SQL | Declarative, English-like keywords, no control flow |

Plus **APL or J as a probe, not a donor** — a density extreme, to find out whether maximal terseness
helps or collapses. Cheap to include in trials; excluded from corpus mining.

---

## 3. Research questions

- **RQ1 — Reliability.** Which surface forms does a model get *right first try*, and what is the
  expected token cost including retries?
- **RQ2 — Transferability.** How much of RQ1's answer is structure, and how much is training prior?
  A property that only holds for familiar syntax is worthless to us.
- **RQ3 — Learnability.** How fast does accuracy on an unfamiliar form rise with in-context examples?
  Our language will always arrive with a spec in context (until anyone fine-tunes on it), so
  *accuracy per token of spec* is arguably the single most important metric in the study.
- **RQ4 — Locality.** Which forms survive being read as a fragment, with no access to the rest of
  the program?
- **RQ5 — Editability.** Which forms make a change cheap to express and hard to get wrong?
- **RQ6 — Projection policy.** How much does what-we-show beat how-we-spell-it?

---

## 4. The instrument that makes RQ2 answerable

For each design axis, the same semantic content is expressed three ways:

| Arm | What it is | What it isolates |
|---|---|---|
| **A. Familiar** | The mainstream form (`if (x) { ... }`) | Structure **+** full training prior |
| **B. Renamed isomorph** | Identical structure, unfamiliar lexemes (`whn (x) { ... }`) | Structure, prior mostly stripped |
| **C. Restructured** | A genuinely different strategy for the same meaning | A different structure, also unfamiliar |

**A − B** measures pure familiarity: same shape, different spelling. **B − C** compares structures
with familiarity roughly held down — the comparison our language actually lives in. Any axis where
A beats B by a lot and B ≈ C is telling us "the model just knows this dialect", and its apparent
advantage will not survive into a new language.

Every axis is also run at 0 / 1 / 3 / 8 in-context examples (RQ3). A form whose accuracy at 3
examples approaches its familiar counterpart is safe to adopt. One that stays flat is a trap, no
matter how token-cheap it looks.

---

## 5. Design axes to test

Each axis is a decision we will have to make. Variants in brackets.

**Structure and delimiting**
1. Block delimiting [braces · indentation · S-expressions · `end` keywords · named closers `end fn foo`]
2. Statement separation [newline · `;` · both]
3. Redundancy at closers [anonymous `}` · closer names what it closes] — does redundancy buy error recovery, or just cost tokens?
4. Formatting [pretty-printed · minified · one statement per line]

**Naming and reference**
5. Identifier form [descriptive · short · opaque IDs · opaque IDs + inline gloss]
6. Reference resolution [callee signature inlined at the call site · lookup elsewhere]
7. Node addressing [no IDs · stable ID on every node] — pays for itself only if it makes edits better

**Types and contracts**
8. Type annotations [absent · inferred-and-shown · required-explicit]
9. Annotation placement [prefix · postfix · separate declaration line]

**Calls and control**
10. Argument passing [positional · named · named above arity 3]
11. Keyword vs sigil for the ten most common constructs
12. Uniformity [exactly one form per concept · multiple sugars]
13. Ordering [signature-first vs body-first; declare-before-use vs any order]

**Failure and absence**
14. Error handling [exceptions · `Result` + `?` · multi-return check] — graded on *omission rate*, the known failure mode
15. Nullability [nullable by default · explicit optional]

**Projection (RQ6)**
16. Slice policy [whole file · slice + referenced signatures · slice + summaries · retrieval on demand]
17. Edit protocol [textual patch · whole-node replacement · structural edit ops]

---

## 6. How trials are graded — no model judges

Every trial is machine-graded, which keeps cost near the generation cost alone:

1. **Parses** — against our own grammar for the variant.
2. **Semantically equal** — canonicalise both to a normal form and compare; independent of spelling
   and whitespace.
3. **Executes** — for variants we can lower to a runnable target, run the reference assertions.

Grade 3 > 2 > 1 is recorded per trial. Structured outputs (`output_config.format`) keep responses in
a fixed shape so extraction never needs a second model call.

**Headline metric: corrected tokens per success** — expected total tokens to reach a correct result,
including retries. A form that's 30% cheaper per attempt and fails a third of the time is not cheaper.

Secondary: edit correctness, long-range closure rate (structure still balanced at 200 / 600 / 1,500
lines), learnability slope, and error-omission rate.

---

## 7. Cost — this is much cheaper than I implied last time

I said the reliability probe "roughly doubles the work". That was true of engineering time and
wrong about money, and money is what you asked about. Corrected estimate:

| Component | Cells | Trials | Est. input tok | Est. output tok |
|---|---:|---:|---:|---:|
| Core axes (15 axes × ~3 variants × 12 tasks × 6 samples) | 45 | 3,240 | ~3.9 M | ~0.5 M |
| Learnability curves (4 example-counts on 8 key axes) | 24 | 1,150 | ~1.6 M | ~0.2 M |
| Long-range closure (3 axes, 1.5k-line programs) | 9 | 360 | ~7.2 M | ~0.7 M |
| Edit + projection trials (axes 16–17) | 12 | 900 | ~5.0 M | ~0.3 M |
| **Total** | | **~5,650** | **~17.7 M** | **~1.7 M** |

At Claude Opus 5 list rates ($5 / $25 per MTok) that is roughly **$90 input + $43 output ≈ $135**,
before two large discounts that both apply here: the **Batch API at 50%** (nothing is latency
sensitive), and **prompt caching** — every trial in a cell shares an identical spec-and-examples
prefix, which is the ideal caching shape. Realistically **$50–100 for a full pass**, and even with
pilots, reruns, and a second model tier for robustness, **a few hundred dollars**.

That reframes your decision: the probe is not the expensive part. Authoring the grammars, graders
and task set is. Recommendation: **run it.** Without it the study can only report token counts, and
token counts are the part we can already decide by fiat.

Two cost notes for correctness rather than budget:
- **Use `messages.count_tokens` for token measurement here**, not the `o200k_base` proxy the first
  survey used. That proxy was fine for *ranking* languages (the ranking held at ρ = 0.999 across two
  BPEs), but we are now designing an encoding against a specific tokenizer, and proxy error compounds.
- **Test on at least two model tiers** (e.g. Opus 5 and Haiku 4.5). A syntax that only the strongest
  model handles is a bad language — cheap models will be doing most of the bulk edits.

---

## 8. Revised phases

| Phase | Work | Output | Depends on API budget |
|---|---|---|---|
| 1 | Corpus mining, cut to ~20 constructs that map to a design axis, 10 donor languages | The construct cost tables you asked for, grouped by rough cost | No |
| 2 | Author the axis grammars, canonicaliser and graders | `variants/`, `grade/` | No |
| 3 | Pilot: 3 axes end-to-end, verify effect sizes and that grading is sound | Pilot report, revised sample sizes | ~$10 |
| 4 | Full trial run, batched | `data/trials.csv`, per-axis effect sizes with A−B / B−C decomposition | ~$50–100 |
| 5 | Learnability curves and long-range closure | Adoption-risk rating per axis | included above |
| 6 | Projection and edit-protocol trials | Recommendation for the read view and edit protocol | included above |
| 7 | Synthesise into a candidate v0 syntax + projection spec, with the evidence for each choice | `SYNTAX-v0.md` | No |

Phase 1 is what the earlier plan was entirely about; it is now one cheap phase whose main job is to
generate hypotheses for the axes, plus deliver the construct tables as asked.

---

## 9. What I dropped from the previous plan, and why

- **25 languages → 10 donors.** Popularity coverage buys nothing here; strategy coverage does.
- **49 constructs → ~20.** A construct earns its place only if it maps to a design axis. Measuring
  `while` loops in fourteen languages does not change any decision we will make.
- **The friction rubric.** It was a proxy for context-dependence and error-proneness. The trials now
  measure both directly, and a measurement beats a scored opinion.
- **Per-language kata matrices.** Replaced by per-axis variants, which is the same authoring effort
  pointed at questions we actually have to answer.

## 10. Risks

- ~~**We design for today's models.**~~ Retired by `ADR-001-model-as-target.md`: a model is a
  compilation target, so model-specific findings are parameters rather than a shelf-life problem.
  The study's output changes accordingly — a knob space and a fitting procedure, not one winning
  syntax. The expensive risk moves to *core rot* (L0/L1 changes invalidate every profile).
- **Out-of-distribution penalty is real.** Our language will, at first, underperform Python on the
  same task for the same model — the prior is enormous and we are throwing it away. RQ2/RQ3 exist to
  size that penalty honestly before we commit. If the A−B gap turns out to dominate everything else,
  the correct design is "mimic familiar surface forms closely and spend the savings elsewhere", and
  the study should be allowed to reach that conclusion.
- **Grader bugs look like findings.** A canonicaliser that treats two different programs as equal
  inflates every variant it touches. Graders get their own test suite with deliberately wrong inputs
  before any trial runs.
- **Cell effects can be task artefacts.** Tasks are drawn across construct families and
  counterbalanced; any axis whose result rests on a single task family is reported as provisional.
