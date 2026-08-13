# Y: a language for code that humans will not read

**Status:** proposal, v0. Nothing here is implemented.

## 1. The premise

Assume the coding is done by frontier-class models — the "Opus 5 High and above"
bar — and that humans do exactly two things:

1. set direction, and
2. review a *sample* of what comes back.

Every decision in this document follows from taking that seriously rather than
treating it as a productivity add-on to a language designed for human authors.

Note what inverts. For fifty years language design has optimized the cost of
*producing* code: terseness, inference, ergonomics, familiarity, developer
happiness. Production is now close to free. The scarce resource is human
attention, and it is scarce by roughly three orders of magnitude — a model emits
twelve thousand lines in an afternoon and a reviewer will genuinely read sixty.

So the design problem is not "how do we write code faster." It is:

> **How do we make reading 1% of a change tell us something true about the other 99%?**

That is the whole thesis. Everything below is downstream of it.

## 2. Why sampling fails in every language we have

Reviewing a random 3% of a diff in Go, Python, TypeScript, or Rust tells you
almost nothing, for one precise reason: **correctness is not local.** A function
can be flawless on the page and wrong in the program, because

- it reads ambient or global state invisible from its text;
- it can perform I/O, block, allocate without bound, or panic, none of which
  appear in its signature;
- a caller you did not read violates an invariant it silently assumes;
- a dependency five levels down shadows, patches, or reconfigures it;
- its failure behaviour is conventional rather than declared.

Under those conditions a "sample" is not a sample. There is no population-level
property being estimated, so reading 3% bounds 3%. Sampling review in today's
languages is a ritual, not a measurement.

Y's answer is to make each unit self-contained *in its claims*, make composition
truth-preserving, and let the machine check everything nobody reads. Then a
sample estimates a real quantity — and, more usefully, the machine can tell you
*which* 1% is worth your eyes.

## 3. Six principles

### 3.1 Everything a unit can do is in its contract

A signature declares types, effects, failure modes, resource bounds, and
pre/postconditions. If it is not in the contract, it cannot happen — this is
enforced, not aspirational.

Effects and bounds are both *declared* and *inferred*, and a mismatch is a
compile error. That redundancy would be an unacceptable typing tax for a human
author; for a generator it is free, and it converts a whole class of model
mistakes into compile failures. **Redundancy is cheap when the writer is a
machine, and redundancy is how you detect its errors.**

### 3.2 No action at a distance

- No global mutable state.
- No ambient authority. Capabilities are ordinary values, passed explicitly.
- No implicit conversion, no overload resolution that depends on distant context.
- No reflection, no monkeypatching, no dynamic code loading.
- No inheritance — composition only, so reasoning stays linear.
- Name resolution is lexical and unit-local: what a name refers to is
  determined by the unit plus its import block. No glob imports, ever.

The consequence is the precondition for everything else: **a unit can be
understood by reading the unit.**

### 3.3 The reviewable artifact is not the implementation

Y is three layers, with sharply different authorship and review economics.

| Layer | Author | Size | Human review |
|---|---|---|---|
| **Policy** — architecture, invariants, capability grants, budgets | human | hundreds of lines for a whole system | always, in full |
| **Contract** — signatures, effects, failures, costs, properties | model | ~10% of the code | the primary sampling surface |
| **Implementation** — bodies | model | the rest | only where verification is weak |

Strategic guidance needs somewhere to live that is not prose in a ticket. Policy
is that place: the human writes constraints, the machine obeys them and proves
it did.

### 3.4 Diffs are semantic, not textual

The compiler classifies every change:

- `behavior-preserving` — proved; refactors, renames, reorganizations
- `internal` — body changed, contract identical, properties still hold
- `contract-widening` / `contract-narrowing` / `contract-breaking`
- `capability-change` — always mandatory reading
- `policy-exception` — always mandatory reading

A twelve-thousand-line PR that is 90% mechanical refactor should present to a
human as sixty lines. Producing that reduction is a language-level obligation,
not a tooling nicety.

### 3.5 The compiler allocates human attention

Every unit carries a verification level — `proved`, `property-covered`,
`contract-checked`, `unverified` — combined with blast radius: capabilities
held, reachability from a trust boundary, policy criticality. That yields a risk
weight per changed unit, and `y review` emits a ranked reading list that
maximizes risk covered per line read, then reports **coverage of risk-weighted
change** rather than the meaningless "lines reviewed."

Sampling stops being uniform and becomes targeted. This tool is the point of the
language, and it is only sound because of §3.1–3.3. You cannot bolt it onto a
language that has escape hatches, because a single invisible escape hatch
invalidates every inference the sampler makes.

### 3.6 Optimize for machine generation, not human typing

- **One canonical form.** The toolchain rewrites to it on save; non-canonical
  text never reaches the repository. Formatting is not a preference, and diffs
  never carry stylistic noise.
- **A small orthogonal core.** Exactly one way to express each thing. This
  lowers model uncertainty, raises output consistency, and — critically — makes
  samples representative of the whole.
- **Explicit at boundaries, inferred inside bodies.**
- **No macros.** Code generation is a build step with declared inputs and
  outputs, and the generated code is what gets reviewed.
- **Errors are structured data** with machine-actionable repair suggestions and
  shrunk counterexamples, because the primary consumer of a compiler error is a
  model in a repair loop, not a person squinting at a terminal.
- **Breaking changes are cheap now.** Every language change ships with a
  mechanical migration and the toolchain rewrites all code in the world.
  Migration cost is the reason languages ossify; that cost has collapsed, so Y
  is designed to stay clean permanently rather than accumulate compatibility
  sediment.

## 4. Capabilities as the fixed review surface

Ambient authority is what makes a model-authored codebase genuinely dangerous: a
hallucinated import, a compromised transitive dependency, a grant that is one
wildcard too wide. In Y no code performs an effect without holding a capability
value granted explicitly and passed down an explicit chain from `main`.

This buys something specific and valuable: the *complete* authority surface of a
large system is a few dozen lines, it is enumerable, and it is the one thing a
human reads on every single PR regardless of sampling. Any widening of it is a
reviewed event by construction.

## 5. Specification by property, not by example

Properties are the primary correctness artifact, because a property is a
compressed review of infinitely many behaviours — exactly the leverage a
sampling reviewer needs. Example-based tests exist only as auto-generated
regression pins for counterexamples already found. Nobody writes them by hand;
the machine generates cases, shrinks failures, and pins them.

## 6. Provenance is in the language, not in comments

Every unit records the model and spec that produced it, its verification status,
its human-review status and date, and the policy version it was checked against.
This is queryable:

```
y query 'holds_capability(net.*) and verification < property-covered and not human_reviewed_since(policy.version)'
```

That query is how you aim a review at 3% of a codebase and know what the other
97% is resting on.

## 7. What it looks like

```y
module payments.refund

use money.{Money, USD}
use ledger.{Ledger, LedgerError}
use audit.Audit

/// Reverse a settled charge.
pub fn issue_refund(
    ledger: &Ledger,
    audit:  &Audit,
    charge: SettledCharge,
    amount: Money<USD>,
) -> Refund

    requires amount > Money::zero
    requires amount <= charge.amount

    ensures  ok  => ledger.balance_of(charge.account)
                    == old(ledger.balance_of(charge.account)) - amount
    ensures  err => ledger.unchanged

    effects  [ledger.append, audit.emit]
    raises   [RefundError.Duplicate, RefundError.LedgerUnavailable]
    cost     time O(1), alloc <= 4
{
    // body: model-authored, checked against the above, rarely read
}

property refund_is_idempotent {
    forall c: SettledCharge, a: Money<USD> {
        let first  = issue_refund(ledger, audit, c, a)
        let second = issue_refund(ledger, audit, c, a)
        first.is_ok implies second == err(RefundError.Duplicate)
    }
}
```

And the human-authored policy that governs it:

```y
// policy/architecture.y
layer domain  { depends_on [] }
layer service { depends_on [domain] }
layer adapter { depends_on [service, domain] }

require pub fn in payments.*  { properties >= 1 }
require verification of payments.ledger >= proved

// policy/capabilities.y
grant payments.gateway { net.https("api.stripe.com"), clock.now }
grant payments.refund  { ledger.append, audit.emit }
deny  domain.*         { net.*, fs.*, clock.*, rand.* }
budget grants.total <= 14
```

## 8. What review looks like

```
$ y review --diff main..pr/4812

  417 files   12,884 lines changed

  proved behavior-preserving     11,203 lines   38 units   skip
  internal, contract unchanged      902 lines   61 units   skip
  contract changed                   61 lines   14 units   READ
  capability grants added             2 lines    1 unit    READ
  policy exceptions                   1 line     1 unit    READ
  weakly verified                   715 lines    9 units   SAMPLE 3

  mandatory                    64 lines
  targeted sample             127 lines   (3 of 9 weak units, highest risk)
  ------------------------------------------------
  human surface               191 lines   (1.5% of diff)
  risk-weighted coverage          94%

  residual risk concentrated in:
    payments.gateway.retry  — unverified, holds net.https, never human-reviewed
```

That output is the product. The language exists to make it honest.

## 9. Anti-goals

Y explicitly does not pursue terseness, familiarity to existing languages,
backwards compatibility as a sacred value, expressive metaprogramming, multiple
paradigms, or developer happiness. The developer is a model. Optimizing the
language for its comfort is a category error; optimizing it for its *accuracy*
and for our ability to check its work is the entire job.

There is no invisible escape hatch. FFI and `unsafe` exist, but they are
capability-gated, recorded in provenance, always in the mandatory review set,
and budgeted by policy.

## 10. Open problems

These are real and I would rather state them than have them found in review.

1. **Correlated failure.** The same model writes the spec and the
   implementation, so it can be consistently wrong in both. Mitigations: policy
   is human-authored; contract changes are always reviewed; properties are
   generated adversarially and independently from implementations; metamorphic
   and differential testing. This remains the deepest risk in the design and it
   is not fully solved.
2. **Spec bugs.** A wrong contract verifies happily. The surface is ~10x
   smaller and attention density correspondingly higher, which helps, but does
   not eliminate it.
3. **Proof cost.** Full verification everywhere is unaffordable. Y tiers it:
   types and effects always, contracts checked and fuzzed by default, proofs
   only where policy demands them.
4. **Totality is not always available.** Interpreters, solvers, and schedulers
   cannot declare meaningful bounds. There is an explicit `unbounded` effect,
   and policy decides where it is tolerable.
5. **The existing world.** Every FFI boundary is unverified by construction. Y
   can bound and quarantine it; it cannot verify it.
6. **Canonical-only form is hostile to human authorship.** Accepted
   deliberately, and worth revisiting only if the premise in §1 turns out to be
   wrong.

## 11. Build order

The sequencing matters, because the differentiator is not the type system.

1. Core language, effects, capabilities, canonical form, property runner.
2. **Semantic diff and review targeting.** Early — this is the thesis, and
   building it late would mean designing the language without feedback on the
   only thing it is for.
3. Policy layer and enforcement.
4. Proof tier for units policy marks critical.
5. Provenance, queries, and migration tooling.

---

*Discussion welcome on any section. §3.5 and §10.1 are where I would spend
review attention first — the first is the thesis, and the second is the argument
most likely to sink it.*
