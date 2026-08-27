# ADR-001 — A model is a compilation target

**Status:** accepted as the working frame for the syntax study.
**Supersedes:** the "don't design for today's models" risk in `PLAN-llm-syntax.md` §10.

## The proposal

Treat the binary core as an intermediate representation and each model as a target, the way a
compiler targets multiple CPUs. Model-specific surface syntax is acceptable. The hard requirement:
**different models, interacting with different surface versions, must be working on the same
program.**

## Why this is the right frame

It dissolves the shelf-life problem rather than mitigating it. "This finding is model-specific" stops
being a threat to validity and becomes a *parameter*, exactly as "this scheduling decision is
Skylake-specific" is not a flaw in LLVM. A new model is a new backend, not a migration.

It also disposes of the training-prior confound that the three-arm experiment was built to expose.
If a model has an overwhelming prior for `if (x) { ... }`, the fitting procedure simply selects that
form *for that model*. Prior becomes a per-target cost-model input instead of a global design
constraint we have to fight.

## Where the analogy breaks — and it's the part that matters

**A CPU never writes code back.** Codegen is one-way and correct by construction. Here the target
*edits the program and hands it back*, through a channel with a non-zero error rate.

So a "backend" here is not a renderer. It is a pair:

```
render  : Core × Profile → Surface        (pure, deterministic, versioned)
ingest  : Surface × Profile → Core ⊎ Error (total, validating, verified inverse)
```

The correctness object is the **round trip**, not the rendering. `ingest ∘ render = id` must hold on
the whole core, and `ingest` must reject rather than guess whenever a model hands back something
outside the surface grammar. Everything expensive about this design lives in `ingest`.

Two consequences worth stating before any code exists:

- **Rendering must be deterministic and versioned.** If two invocations render a node differently,
  every downstream diff is noise. Pin `(core, renderer version, profile)` in anything you log.
- **Store cores, never rendered text.** The moment rendered text is authoritative anywhere — a
  review artifact, a test fixture, a cache — you have forked the source of truth.

## The layer boundary — what may vary per target, and what may never

This is the whole ADR. Getting it wrong turns "multiple backends" into "multiple languages".

| Layer | Contents | May vary per model? |
|---|---|---|
| **L0 — Semantic core** | Structure, types, effects, node identity, provenance | **Never.** Single authority for meaning. |
| **L1 — Conceptual vocabulary** | Which constructs exist at all (loop, match, result, effect) | **Never.** If A can express something B cannot, they aren't the same language. |
| **L2 — Surface grammar** | Spelling, delimiters, sugar, ordering, annotation placement | **Yes.** This is the backend's freedom. |
| **L3 — Projection policy** | What subset is shown, how much context, what is summarised vs. elided | **Yes**, and per *task* as well as per model. |

Your "same program" requirement is guaranteed structurally, not by discipline, if and only if L0/L1
are fixed and every surface is a pure function of the core. Two models never reconcile surfaces with
each other — they both reconcile with the core. Semantic equality is decided in L0, by comparing
canonical cores, and never by comparing text.

## Hand-written backends won't survive the model cadence

LLVM can afford hand-written backends because an ISA is stable for a decade and a backend is written
once by a specialist. Frontier models ship every few months, and their preferences will drift within
a family. A hand-written surface per model is a rewrite treadmill.

The replacement, and the thing that makes this proposal practical:

> **The renderer is parameterised, not hand-written. A target profile is a vector of settings over
> the design axes. Adding a model means *fitting* a profile by running the benchmark suite — not
> authoring a backend.**

This is autotuning (ATLAS, FFTW) rather than classical codegen: a knob space plus a search
procedure, where the search is empirical measurement against the actual target.

It also changes what the study in `PLAN-llm-syntax.md` produces. Not "the best delimiter" — but:

1. the **knob space** (which axes matter, which are inert, which interact),
2. the **fitting procedure** (how few trials pin a profile),
3. a **default profile** for the reference surface, and
4. the **spread** — how much profiles actually differ between models, which decides whether
   per-model grammar is worth owning at all.

Item 4 is the decision-relevant one and it comes almost free from the trials already planned.

## The cheaper thing you may not need to go past

Per-model **L3** (what is shown) and per-model **L2** (how it is spelled) are separable, and the
evidence so far says they are not close in value: choosing what to put in context is a multiple,
choosing keywords is tens of percent.

So the recommended sequencing is:

1. **One shared surface grammar, per-model projection policy.** One `ingest` to write, one to verify,
   one to debug. Most of the benefit.
2. **Per-model grammar only where the trials show an L2 effect large enough to pay for a second
   verified `ingest`.** Let the measured profile spread decide, per axis, not per model.

That keeps N surfaces from becoming N maintenance burdens before you know whether N > 1 earns
anything.

## New measurements this frame requires

Three additions to the trial suite, all mechanical, all cheap:

- **Round-trip fidelity.** For every profile: `ingest(render(core)) == core` over the corpus, plus
  the rejection rate and failure taxonomy for model-authored surfaces that don't parse. This is the
  correctness metric the CPU analogy doesn't supply.
- **Cross-surface agreement.** Same edit instruction, issued to two models through two profiles;
  compare the resulting cores for semantic equality. This measures your stated must-have directly,
  and it is a strong oracle — disagreement localises to a node, not a paragraph.
- **Profile transfer.** Fit a profile on model X, apply it to model Y, measure the loss. Cheap
  transfer means the knob space is coarse and one profile may serve a family; expensive transfer
  means per-model fitting is genuinely load-bearing.

## Concurrency, merge, and review

Two models editing one program through different surfaces is not compilation — it is collaborative
editing over a shared structure. Merge must happen in L0. That is a real cost, and also the largest
incidental win available: structural merge makes most textual conflicts disappear, and renames stop
being diffs at all if node identity is intrinsic rather than name-based.

Code review, blame, and `git diff` all become operations on a projection. Plan for a merge driver
and a diff tool as first-class components, not afterthoughts.

## Prior art worth reading before building

- **WebAssembly (`wasm`/`wat`)** — the closest precedent: a binary core with one canonical text
  projection and a specified mapping between them. A working model for "binary is authoritative, text
  is a view".
- **Unison** — definitions content-addressed by the hash of their AST; names are metadata; the
  codebase is a database, not files. Directly the L0 you are describing, with renaming-is-free and
  no-conflict-on-rename already worked out.
- **JetBrains MPS** — projectional editing in production for ~20 years. Its known pain is that
  humans dislike editing through a projection; your humans are read-only, which removes most of it.
  Its lessons about tooling surface still apply in full.
- **Dark** — structured editing plus no build step; cautionary tale about how much editor and
  tooling you end up owning.
- **JVM bytecode / LLVM IR** — the IR discipline itself: an IR that isn't rigorously specified and
  version-stable rots every backend attached to it.

## Revised risks

- ~~Findings have a shelf life~~ → **replaced by profile drift**, which is cheap: re-fit by re-running
  the suite (order $50–100), no corpus migration.
- **Core rot** is now the expensive risk. L0/L1 changes invalidate every profile and every stored
  program. The core needs a specification and a versioning policy before the second backend exists.
- **`ingest` is the sharp edge.** A permissive parser that "helpfully" repairs malformed model output
  silently admits programs the model didn't mean. Reject and re-prompt; never guess.
- **Profiles fitted on benchmarks may not transfer to your real code's idioms.** Fit on a sample
  drawn from the actual codebase, and re-fit when the codebase's character shifts.
- **Per-model surfaces make cross-model evaluation harder** — you can no longer compare two models on
  one benchmark surface. Keep the reference profile as the common measuring stick.
