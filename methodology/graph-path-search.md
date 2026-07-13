# Graph Path Search

A methodology for treating an open question in physics as a **search for a
path — or a minimal set of paths of a particular nature — through the
dependency graph** that [open-question dependency
mapping](./open-question-dependency-mapping.md) produces.

Where dependency mapping *builds* the graph, path search *reasons over
it*. It is the method we have been using to describe the chain between GoE
and our present laws of physics, and the method behind [the Grand
Convergence](../definitions/grand-convergence.md).

## The graph

Open-question dependency mapping gives us a graph of the laws of physics as
we currently understand them through the eyes of PWH: the nodes are the
questions, patterns, and formula-groups; the edges are the dependencies
between them ("flows through", "is explained by", "evolves into"). For any
two nodes there may exist a path that travels between them.

Two very different questions can be asked of such a graph, and they turn
out to be the same question asked under different amounts of knowledge:

1. Given a graph we know well, and endpoints we have already identified,
   what is the *minimal, most-shared* path structure connecting them?
2. Given a graph we know only partially, which paths are even *admissible*,
   and what do the admissible ones have in common?

Both are path search. The first optimizes a path we can already see; the
second discovers a path we mostly can't. This document treats them as one
methodology with two regimes.

## Regime 1 — known graph, known endpoints: convergence

Here we know the source(s) and destination(s) up front, and often a
mandatory intermediate node that every path is required to pass through. We
are not merely asking *whether* a path exists. We are looking for the
**minimal set of paths, all sharing a specific structure, that connect the
endpoints through the required node**.

Phrased in graph terms — the words that were hard to find — this is a
**constrained minimal-subgraph problem**: find the smallest subgraph (a set
of paths) that connects a chosen set of **terminal** nodes while routing
*all* of them through a single required **waypoint** (a mandatory
cut-vertex / articulation node). It is Steiner-tree-like — connect a set of
terminals through shared intermediary nodes, minimizing the connecting
structure — with the extra constraint that one particular node lies on
every path. Because all paths are forced to funnel through that one node,
they **converge** on it: the waypoint stops being just an intermediary and
becomes a **common substrate** the terminals are all re-expressed in terms
of.

That is exactly what the Grand Convergence was:

- **terminals** — the several groups of formulas we wanted to unify;
- **mandatory waypoint** — the phase wave, the known common denominator
  every path had to travel through;
- **result** — the minimal converging path-set that let those formula-groups
  be re-expressed over a small set of identical currencies, revealing that
  they were formulas for the *same ontological part of physics*.

So the "specific nature" of that exercise was **convergence**: not any set
of connecting paths, but the minimal set that funnels every terminal
through a shared node and thereby collapses them onto one substrate.

## Regime 2 — partial graph: discovery under constraint

Often we do not have the whole graph. The laws of physics are the paradigm
case.

The laws of physics *at a given time* are the collection of stable
phase-wave patterns available at that time. If we knew all the patterns, we
would know all of physics. But we currently know neither the full set of
patterns nor the **evolutionary path** by which they arose — how physics
evolved from GoE, the beginning of time, to the physics we measure now.

Crucially, **not knowing the whole graph does not leave the answer
unconstrained.** Partial knowledge still lets us:

- **identify individual nodes** — a specific stable pattern, or a specific
  evolutionary step — and place it, at least partially, in the overall
  picture;
- **constrain which paths are admissible**, by applying heuristics that
  prune nodes and edges the true path cannot contain;
- **pin nodes** we know any valid path must traverse, even when we cannot
  yet see the segments on either side of them.

Path search here is therefore **discovery under constraint**: searching for
admissible paths through a graph we only partially know, and tightening the
search with every constraint we can justify. It is the same convergence
instinct as Regime 1, run against a graph that is still mostly hidden.

## The moves

A search, in either regime, proceeds by repeatedly applying a small set of
moves — each one shrinking the space of possible answers:

- **Fix the terminals.** Choose the source and destination you care about
  (e.g. GoE → a specific present-day law). This bounds what "a path" even
  means.
- **Pin the mandatory nodes.** Assert a node every valid path must pass
  through (a known pattern, a known intermediate law, the phase wave). Each
  pin is a cut-vertex that partitions the search and discards every path
  that misses it.
- **Apply heuristics.** Impose constraints that prune or bias nodes and
  edges — stability requirements on a pattern, invariants that must be
  conserved, ordering in evolutionary time. A heuristic need not be exact;
  it only has to shrink or bias the admissible set.
- **Seek the minimal, converging path-set.** Among the paths that survive,
  prefer the smallest, most-shared structure — the one that funnels the
  most terminals through the fewest shared nodes. Convergence is the target
  even when the graph is incomplete.
- **Iterate.** Every identified node, every pin, every new heuristic shrinks
  both the path and the set of possible solutions. Constraints only
  accumulate; the admissible space only narrows.

## In one sentence

We continuously **shrink the path and the space of its possible
solutions** — by fixing terminals, pinning the nodes a valid path must
cross, and applying heuristics that prune the rest — closing in, step by
step, on the true structure of the laws of physics.

## Relationship to the other methods

- [Open-question dependency mapping](./open-question-dependency-mapping.md)
  *builds* the graph this method searches. The two compose: you cannot
  search a graph you have not mapped, and a map is only as useful as the
  paths you can find through it.
- [The Grand Convergence](../definitions/grand-convergence.md) is a
  completed instance of Regime 1 — a search that found the minimal
  converging path-set through a region of the graph we knew well enough,
  with the phase wave as the mandatory waypoint.

## Glossary

| Graph term | In this framework |
| --- | --- |
| node | an open question, a stable phase-wave pattern, or a law / formula-group (through PWH) |
| edge | a dependency: "flows through", "is explained by", "evolves into" |
| path | a chain of dependencies linking two nodes |
| terminal | a node you are trying to connect — a chosen source or destination |
| waypoint / cut-vertex | a node every admissible path is required to traverse (e.g. the phase wave) |
| converging path-set | the minimal subgraph connecting the terminals through a shared node |
| common substrate | what a waypoint becomes once every terminal is re-expressed through it (e.g. the identical currencies of the Grand Convergence) |
| heuristic | a constraint that prunes or biases admissible paths without having to be exact |
| partial graph | the laws of physics as currently known — a few nodes and edges known, most still hidden |
