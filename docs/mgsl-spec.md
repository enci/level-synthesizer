# MGSL — Multi Grid Scripting Language

**Status**: Draft v0.1
**File extension**: `.mgsl`
**Scope**: Core language semantics for grid-based procedural generation.

MGSL is a domain-specific language for procedural generation of grid-based content (roguelike dungeons, puzzle layouts, tile maps). Programs describe how to transform a stack of correlated grids through a sequence of pattern-rewrite rules.

---

## 1. Design principles

1. **Declarative**: the program describes *what to match and write*, not *how to iterate*.
2. **Visual**: pattern bodies look like the grids they match; the editor can render them with colored backgrounds.
3. **Minimal surface area**: a small set of orthogonal concepts (tags, grids, rules, programs) covers the design space.
4. **Predictable execution**: double-buffered rule application; one well-defined collect-then-pick model for all strategies.
5. **Cognitive load over terseness**: a rule should read clearly in isolation, even if that costs a few characters. Boilerplate is required only when ambiguity would otherwise exist.

---

## 2. Lexical structure

### 2.1 Source files

MGSL source files have the extension `.mgsl` and are UTF-8 encoded. Unicode characters are first-class in pattern bodies (tag glyphs are typically Unicode) and in identifiers (see §2.3).

### 2.2 Comments

```
// single-line comment
```

Block comments are not supported.

### 2.3 Identifiers

Identifiers follow [Unicode Standard Annex #31](https://www.unicode.org/reports/tr31/):

- **Start character**: any Unicode character with `XID_Start` (letters in any script) or `_`.
- **Continue character**: any Unicode character with `XID_Continue` (letters, digits, connector punctuation) or `_`.

So `wall`, `墙`, `Mauer`, `_internal` are all valid identifier names. ASCII identifiers remain idiomatic for shared codebases but are not required.

This rule applies to tag names, tag value names, grid names, and rule names alike.

### 2.4 Reserved keywords

```
tag  layers  grid  of  number
rule  program
any  all  one  some
repeat  resize  upscale
symmetry  rotation  weight  max
```

Keywords are ASCII; they do not collide with Unicode identifiers.

### 2.5 Pattern tokens

Inside `[ ... ]` pattern bodies, tokens are separated by whitespace and may be:

| Token | Meaning |
|-------|---------|
| `.`   | match any value (including empty) |
| `?`   | match or write the empty cell |
| *identifier* | a tag value name (as declared via `tag`) |
| *single Unicode codepoint* | a tag value glyph alias (as declared via `tag`) |
| *integer literal* | a numeric value (for `grid of number`) |

Tag value glyphs may be any single Unicode codepoint that is not reserved (`.`, `?`, whitespace, `[`, `]`, `,`).

---

## 3. Tag declarations

A `tag` declares a tagset: a closed set of named values that a grid can hold.

```mgsl
tag items    { chest, heart, potion, sword, shield }
tag enemies  { goblin, troll, dragon }
tag geometry { wall, floor }
```

Tag values are referenced by their name in pattern bodies. Two tagsets may define values with the same name; resolution is by the grid's declared tagset.

Optionally, a tag value may carry a Unicode glyph alias:

```mgsl
tag geometry { wall █, floor · }
```

When a glyph alias exists, it may be used interchangeably with the name in patterns. Editor highlighting uses the glyph form for compact rendering.

---

## 4. Grids and the `layers` block

All grids used by a program are declared in a single `layers` block:

```mgsl
layers {
    level:   grid of geometry
    tiles:   grid of number
    enemies: grid of enemies
    items:   grid of items
    algo:    grid of algo
}
```

Each entry is `name: grid of <type>`, where `<type>` is either:
- A tagset name (`grid of geometry`)
- The keyword `number` (for numeric/scalar grids)

Grids have no declared size; size is set at program runtime via `resize`. All grids share the current size at all times.

### 4.1 Empty cells

Every cell of every grid starts empty. Empty is not a tag value; it is the absence of one. It is matched and written via `?`:

```mgsl
algo[?]       // match an empty cell
algo[?]       // (on the RHS) clear the cell
```

Numeric grids treat `0` as a distinct value from empty.

---

## 5. Rules

A rule is a named match-write transformation. Its body declares one or more *match patterns* (left of `=>`) and one or more *write patterns* (right of `=>`).

### 5.1 Basic form

```mgsl
rule start {
    algo[?]
    =>
    algo[S]
}
```

The simplest rule body has a single match and a single write. When a position in the grid matches the LHS, the matched cells are replaced according to the RHS.

### 5.2 When combinators (`any` / `all`) are required

**Combinator blocks are required only when grouping more than one item at the same level.** A rule containing a single match-write pair, with a single pattern on each side, needs no combinator anywhere.

A combinator block is needed for:

- A LHS that conjoins multiple patterns across grids (use `{ all ... }`).
- A RHS that picks among alternatives (use `{ any ... }`).
- A rule body containing multiple independent match-write sub-rules (use either, at the rule body level).

Concretely:

```mgsl
// One match, one write — no combinator needed.
rule start {
    algo[?]
    =>
    algo[S]
}

// Multiple write alternatives — { any } on the RHS.
rule reward {
    algo[S]
    =>
    { any
      items[chest]
      items[heart]
      items[potion]
    }
}

// Cross-grid match AND alternative writes.
rule place_enemies {
    { all
      algo[F]
      enemies[?]
    }
    =>
    { any
      enemies[goblin]
      enemies[troll]
      enemies[dragon]
    }
}

// Multiple independent sub-rules — combinator at the rule body level.
rule fill_geo { all
    algo[W] => level[wall]
    algo[F] => level[floor]
    algo[S] => level[floor]
}
```

There is no default combinator: in any context that contains more than one item, `any` or `all` must be stated explicitly. A context with exactly one item must omit the combinator.

### 5.3 Pattern bodies

A pattern body is a rectangular grid of cells enclosed in `[ ]`. Whitespace between cells is required; line breaks within `[ ... ]` denote row breaks.

```mgsl
algo[
    . . .
    . S W
    . . . ]
```

The above is a 3×3 pattern. The center cell must be `S`, the right-center must be `W`, and the eight cells marked `.` match anything (including empty).

Single-cell patterns are written inline: `algo[S]`.

Multi-row pattern grammar:
- Empty rows are illegal.
- Trailing whitespace on a row is ignored.
- The closing `]` may be on its own line or at the end of the last row.
- Inconsistent column counts across rows are a compile error.

### 5.4 Match-write shape constraint

For each match-write pair, the match pattern and write pattern must have identical dimensions. When the RHS is a `{ any ... }` block, every alternative must have those same dimensions. The compiler rejects mismatched shapes.

### 5.5 Weights

Within an `any` block, options may carry a weight:

```mgsl
{ any
  (weight=2) algo[F S]
  (weight=1) algo[S F]
}
```

Weight is per-option, prefixed before the option. Omitted weight defaults to `1`. Weights bias random selection proportionally.

### 5.6 Rule attributes

Rules may declare attributes in parentheses after the rule name:

```mgsl
rule rwalk(symmetry=all) { ... }
rule reduce(symmetry=all, rotation=all) { ... }
```

#### 5.6.1 `symmetry`
Generates additional pattern variants by reflection. Identity is always included.

- `symmetry=none` — only the pattern as written.
- `symmetry=horizontal` — adds horizontal flip.
- `symmetry=vertical` — adds vertical flip.
- `symmetry=all` — adds horizontal, vertical, and both-axis flips.

#### 5.6.2 `rotation`
Generates additional pattern variants by rotation. Identity is always included.

- `rotation=none` — only the pattern as written.
- `rotation=all` — adds 90°, 180°, 270° rotations.

`symmetry` and `rotation` are independent. The full dihedral group D₄ is `rotation=all, symmetry=all` (8 variants).

### 5.7 Match and write footprints

Each pattern carries two compile-time footprints, derived from its cells:

- **Read footprint**: the set of cell offsets the matcher actually reads. A cell contributes to the read footprint if it specifies a value — a tag value, an integer literal, or `?`. Cells marked `.` (any) are *not* in the read footprint, since the matcher never inspects them.
- **Write footprint**: the set of cell offsets the rewriter actually writes. A cell contributes to the write footprint if it specifies a value — a tag value, an integer literal, or `?` (which clears the cell). Cells marked `.` are *not* in the write footprint, since they preserve whatever was matched.

Footprints are defined relative to the pattern's origin (top-left corner). When the rule fires at grid position (x, y), each footprint is translated by (x, y) to identify which grid cells are read or written.

The bounding rectangle of a pattern is its W×H region, but the footprints may be sparse subsets. For example, a 3×3 pattern with `.` in all four corners has read and write footprints of just the five center cells (a plus shape).

Footprints drive the runtime's overlap behavior; see §6.4.

---

## 6. Programs

A `program` block declares the orchestration:

```mgsl
program {
    resize(60, 40)
    some(max=5)   start
    some(max=100) rwalk
    upscale(2, 2)
    all reduce
    all reward
    all fill_geo
    some(max=10) place_enemies
    all decorate
}
```

Statements execute top to bottom. There is exactly one program per file (for now).

### 6.1 `resize(W, H)`

Sets the active grid size to W columns by H rows. All grids are sized to this. On first call, every cell is initialized empty. On subsequent calls, behavior is implementation-defined (typically: clears all grids).

### 6.2 `upscale(N, M)`

Multiplies the current grid size by N in width and M in height. Every existing cell is duplicated into an N×M block across all layers, preserving its value. Empty cells remain empty after upscale.

### 6.3 Rule application semantics

Every rule application uses the same **iteration model**:

1. Begin with an empty *overlap mask* the size of the current grid (all cells unmarked).
2. Maintain a *cursor* that yields candidate origin positions in an order defined by the strategy.
3. For each candidate position (x, y):
   a. Compute the candidate's translated read and write footprints (§5.7).
   b. If any cell in the union of those footprints intersects a marked cell in the overlap mask, **skip** this candidate.
   c. Otherwise, test whether the rule's LHS matches at (x, y) against the grid snapshot (§7.1).
   d. If it matches: apply the write to the live grid, then mark every cell in this candidate's write footprint in the overlap mask. Count one application.
4. Stop when the cursor is exhausted *or* the application count reaches K.

The strategies differ in cursor order and K:

| Statement | Cursor order | K |
|-----------|--------------|---|
| `one R` | random | 1 |
| `some(max=N) R` | random | up to N |
| `all R` | linear (top-left to bottom-right) | unbounded |

The overlap mask is **per-statement**: it starts empty at each `one`, `some`, or `all`, and is discarded when the statement completes.

For rules with multiple match variants (from `symmetry` or `rotation` attributes), each candidate position is tested against every variant; the first matching variant is the one applied. Variants do not have separate cursor positions.

For rules whose body uses `{ any ... }` on the write side, the variant *and* the write alternative are chosen at the candidate; both follow the rule's random or weighted selection within the same iteration step.

### 6.4 Overlap behavior

Overlap behavior follows directly from the iteration model in §6.3:

- A candidate is skipped if any cell in its read or write footprint has been marked by a *prior* application in this statement.
- Skipped candidates **do not** consume the application budget (K). The cursor continues until either K applications occur or the cursor exhausts.
- Within `all`, the cursor's linear order makes the result deterministic. Different cursor orders would produce different subsets of applied matches, but linear order is fixed.

Footprint intersection is computed **per cell**, not per bounding rectangle. Two candidates whose bounding rectangles overlap may still both apply, provided their overlapping region consists entirely of cells that are `.` on both LHS and RHS in both patterns (and thus not in either footprint).

The overlap mask is the only mechanism preventing conflicting writes within a statement. The runtime does not detect or special-case other forms of "conflict."

### 6.5 Termination

Each strategy completes in bounded time, because the cursor visits each grid position at most once:

- `one R`: returns after the first successful application, or after the cursor exhausts.
- `some(max=N) R`: returns after N successful applications, or after the cursor exhausts.
- `all R`: returns after the cursor exhausts (every position has been considered exactly once).

There is no "until stable" iteration built into a single statement. To run a rule repeatedly to fixpoint-ish behavior, compose statements in the program (e.g., `some(max=100) r` for bounded iteration).

---

## 7. Execution model

### 7.1 Snapshot semantics

Within a single rule application statement, all matches are evaluated against a **snapshot** of the grid as it was at statement entry. Writes from successful applications accumulate into the live grid and do *not* become visible to subsequent matches in the same statement.

Implementations may realize this as a pre-statement full copy, copy-on-write per touched cell, or any equivalent mechanism. The observable behavior is:

- All matches in one statement see the same input state.
- Writes from earlier successful applications in the same statement are not visible to later matches in the same statement.
- At end-of-statement, the live grid (with all accumulated writes) becomes the snapshot input for the next statement.

This means a cell rewritten by one application of a rule will not be re-matched by the same rule within the same statement, regardless of overlap. To produce iterated behavior, compose multiple statements in the program.

The snapshot mechanism is independent of the overlap mask (§6.4). The snapshot guarantees match consistency *across* candidates within a statement; the overlap mask prevents conflicting *writes* among the candidates that have already succeeded.

### 7.2 Determinism

Given the same random seed, a program produces the same output. The seed is part of the runtime, not the program.

### 7.3 Compile-time checks

The compiler must reject:

- Pattern bodies with inconsistent dimensions across LHS/RHS or across alternatives within an `any` block.
- Tag values not declared in the grid's tagset.
- Combinator blocks with zero or one item.
- Bare lists of more than one item where a combinator is required (no implicit `any` or `all`).
- References to undeclared grids or tags.

---

## 8. Worked example

A complete Nuclear Throne style dungeon generator (see `dungeon.mgsl`):

```mgsl
tag items     { chest, heart, potion, sword, shield }
tag enemies   { goblin, troll, dragon }
tag algo      { F, W, S }
tag geometry  { wall, floor }

layers {
    level:   grid of geometry
    tiles:   grid of number
    enemies: grid of enemies
    items:   grid of items
    algo:    grid of algo
}

rule start {
    algo[?]
    =>
    algo[S]
}

rule rwalk(symmetry=all) {
    algo[
        . . .
        . S W
        . . . ]
    =>
    { any
      (weight=2) algo[
          . . .
          . F S
          . . . ]
      (weight=1) algo[
          . S .
          . F .
          . . . ]
    }
}

rule reduce(symmetry=all) {
    algo[
        S S
        S S ]
    =>
    algo[
        S F
        F F ]
}

rule reward {
    algo[S]
    =>
    { any
      items[chest]
      items[heart]
      items[potion]
      items[sword]
      items[shield]
    }
}

rule fill_geo { all
    algo[W] => level[wall]
    algo[F] => level[floor]
    algo[S] => level[floor]
}

rule place_enemies {
    { all
      algo[F]
      enemies[?]
    }
    =>
    { any
      enemies[goblin]
      enemies[troll]
      enemies[dragon]
    }
}

rule decorate { all
    level[floor] => tiles[1]
    level[wall]  => tiles[2]
}

program {
    resize(60, 40)
    some(max=5)   start
    some(max=100) rwalk
    upscale(2, 2)
    all reduce
    all reward
    all fill_geo
    some(max=10) place_enemies
    all decorate
}
```

---

## 9. Open questions

These are decisions deferred from the design conversation, to be resolved before the language is considered stable.

> **Resolved since first draft**:
> - *`some(max=N)` semantics* — settled as iterated (cursor-based), see §6.3.
> - *Conflict resolution policy* — settled as per-cell overlap masking, see §6.4.

### OQ1. Templates

Mentioned but not specified: named constant grids that can be stamped by rules.

```mgsl
template Vault3x3 { ... 3x3 grid literal ... }
```

Usage in rules requires a `stamp` operation. Deferred.

### OQ2. Imports

Mentioned but not specified: `use "path/file.mgsl"` to bring in templates and tag declarations from other files. Necessary for Spelunky-style chunk libraries. Deferred.

### OQ3. Graphs

The language is designed to leave room for graph layers alongside grid layers in the future. Graph rewrite semantics, syntax, and integration with grid-derived graphs are deferred to a future spec.

### OQ4. Numeric grids

`grid of number` is in the spec but the operations on numeric grids (comparison, arithmetic, fields computed from other grids like flood-fill distance) are not specified. The spec currently treats numeric grids as opaque write-only sinks (e.g., `tiles[1]` for rendering tags).

### OQ5. Rule attribute extensibility

`symmetry` and `rotation` are the only attributes specified. Future attributes (e.g., `probability` for stochastic application, `priority` for ordering) need a place. The parenthesized attribute syntax accommodates this without grammar changes.

### OQ6. Multiple programs per file

Currently one program per file. Multiple named programs (so a single file can be a library of generators) is a likely future need.

### OQ7. Identifier glyph aliases

Tag value glyph aliases (`wall █`) are stored in source as text. The editor decides rendering. The exact LSP protocol for communicating glyph aliases between server and editor is deferred.

---

## 10. Implementation guidance

This section is non-normative; it captures decisions about *how* to build MGSL, made during the design conversation.

### 10.1 Architecture

- **Compiler and interpreter**: C++. Performs parsing, semantic analysis, rule compilation (including symmetry/rotation expansion), and execution. Emits JSON diagnostics on stdout for tooling.
- **Language server**: TypeScript, separate process. Provides editor features (highlighting, diagnostics, hover, completion). On save, shells out to the C++ compiler for ground-truth diagnostics; on keystroke, runs its own tolerant parser for low-latency feedback.
- **Boundary**: JSON. The C++ side never knows the LSP exists.
- **No FFI, no WASM, no native Node module**: subprocess only. Startup overhead is negligible at human timescales.

### 10.2 Editor rendering

Tag values render with background colors in the editor, using VS Code's `TextEditorDecorationType`. Each tag value gets a background color (theme-aware) and a foreground color chosen for contrast. Patterns appear as colored grids while remaining plain text.

Multi-row pattern bodies render naturally because the editor is already monospace.

Multi-character tag values (e.g., the literal name `wall`) are colored in full; single-character glyphs (e.g., `█`) render as single colored cells.

### 10.3 Symmetry expansion

Symmetry and rotation are expanded at compile time. A rule with `rotation=all, symmetry=all` produces 8 internal pattern variants per declared match pattern. The runtime treats each variant as an independent pattern to match.

### 10.4 Snapshot implementation

The per-statement snapshot (§7.1) is implementable several ways. In order of increasing sophistication:

- **Full copy**: clone the entire grid at statement entry; matches read the clone; writes go to the live grid. Simple and correct. Memory cost is one extra grid per statement.
- **Copy-on-write per cell**: maintain a side table of "cells written this statement"; matches check the side table for a pre-statement value, falling back to the live grid. Cheaper when few cells are written.
- **Per-grid versioning**: each cell carries a write-version; matches read the version-N value, writes increment to N+1. Closer to a database MVCC approach.

For v1, a full copy is recommended. Optimization can come later if profiling indicates the snapshot cost matters.

### 10.5 Cursor implementation

The iteration model in §6.3 needs a cursor that yields positions in either random or linear order, with early termination.

- **Linear cursor** (`all`): a simple double loop over (y, x). O(W×H) positions visited.
- **Random cursor** (`one`, `some`): a shuffled list of positions, consumed in order. Construct the shuffle lazily using a Fisher-Yates-style sampler, so `one R` doesn't pay the cost of shuffling W×H positions when it only needs the first few.

For the overlap mask, a packed `bitset` of size W×H is sufficient. Reset (or freshly allocate) per statement.

### 10.6 Match enumeration

For small patterns and small grids (the common case), a per-position brute-force match is adequate. The cursor yields candidate origins; for each, the pattern is tested cell-by-cell against the snapshot, short-circuiting on first mismatch. Optimization (rete-style indexing, bitmap-accelerated scans) is deferred until profiling justifies it.

### 10.7 Random seeding

The runtime accepts a seed via CLI flag. Internally, a single PRNG drives all stochastic decisions (cursor shuffling, weighted choice within `any` blocks). Determinism is per-seed.

---

## 11. Versioning

This document is **v0.1**. Future revisions will note breaking changes. MGSL is pre-1.0; expect changes.
