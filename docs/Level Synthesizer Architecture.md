# Fork Architecture — Reference & Implementation Seed

A node-based procedural level generator (LevelSynth) re-architected from a
**node-graph dataflow** model into a **linear pipeline of heterogeneous
transforms over a shared, typed layer stack** — a blackboard model. This
document is the seed for implementation. It carries the decisions, the
*reasons* behind them, and the deliberately-deferred seams, so that work can
start without relitigating settled questions and so new things can surface
during build.

> **Status.** Design-conversation synthesis, pre-implementation. Where this
> disagrees with code once code exists, code wins and this gets updated.
> Project name is **not yet chosen** (see §12). Inherited LevelSynth section
> references (e.g. "§9") and MGSL spec references (e.g. "MGSL §6.4") point at
> the two companion documents.

---

## 1. What this fork is, in one paragraph

We replace *grids-as-values-flowing-through-a-DAG* with *a fixed set of
co-sized named layers mutated in sequence by an ordered list of transforms*.
The shared layer stack is a blackboard; each transform reads and writes it; the
pipeline is **linear** (no control-flow graph). Pattern-rewrite (the MGSL idea)
becomes **one transform among several**, not the whole model — which is what
lets other paradigms (BSP, WFC, mission-graph-to-space) live in the same tool.

The architectural claim, phrased for the paper: *a linear pipeline of
heterogeneous transforms over a shared typed layer stack, deterministic per
seed, with the generation timeline materialized as a free consequence of
linearity.*

## 2. The through-line: this fork is mostly *subtraction*

Almost every decision below **removes** a mechanism the node-graph original
needed. This is the contribution, not an accident, and it should be written up
as such — a reviewer respects "we removed cycle detection because linearity
makes it unnecessary" more than a feature list.

Removed relative to LevelSynth:

- **Cycle detection + topological evaluation** — gone. Termination is now
  structural (each cursor visits each cell at most once; the pipeline is a
  finite linear list).
- **Pins, wires, the node-graph** — gone. Replaced by named layers + an ordered
  transform list.
- **The visitor pattern (`accept`/`node_visitor`)** — gone. Each transform owns
  its own `save`/`load`/`render_ui` (§6).
- **Tag hashing (FNV) + the global tag hierarchy** — gone. Tagsets are flat,
  closed, per-layer, with stable integer IDs (§4).
- **Grid templating** — never introduced; one concrete `int64` grid (§5).
- **Lifetime-management machinery** (RAII handles, pools) for scratch buffers —
  not built; plain call-scoped borrows suffice (§8).

The guiding rule throughout: **as simple as we can get away with**, read as
*simple-to-understand, not simple-to-type*. When the two diverge, pick
understandable — a thesis examiner and a future forker read this. Don't build a
mechanism to defend an invariant the architecture already guarantees.

## 3. What carries over from LevelSynth (intact, in spirit, or as a service)

**Intact:** the dense grid type, per-seed determinism, the library/editor split
(`ls::` library has no ImGui; editor depends on library), unit-test-first
discipline (Catch2, exercise the library directly), and the
`LS_REGISTER_NODE`-style registry macro — *retargeted from nodes to transforms*
(flat list, ~a dozen entries; see §6).

**In spirit:** the `generator` façade — owns a layer stack + compiled pipeline +
seed, exposes `evaluate()` and named-output access. The named-binding idea
(`generator::rebuild_bindings()`) survives as name→index resolution at
compile-time (§7).

**As reusable runtime services (not as a language):** MGSL's hard-won execution
semantics — the per-statement **snapshot** (MGSL §7.1) and the per-cell
**overlap mask** (MGSL §6.4) — survive as services the *rewrite transform*
composes. They are not baked into the transform interface; a BSP transform
ignores them. The MGSL spec becomes the specification *for the rewrite transform
specifically*, not for the tool.

**Tech stack:** C++20 + CMake. SDL3 (window/input/clipboard/prefs), Dear ImGui
(docking branch), FreeType + Phosphor icons, `nlohmann::json` behind a
string-based library API. **`imgui-node-editor` is dropped** — there is no node
canvas anymore; the editor is a pipeline list + a rule painter (§9).

## 4. Tags — resolved

The single resolving insight: **the tagset lives on the layer, not in the
cell.** A grid flowed down a wire in LevelSynth, so each cell had to be
self-describing (hence the 64-bit hierarchical tag). Here, grids are declared,
named, and typed once; the cell can collapse to a small integer interpreted
*through the layer's bound tagset*.

Consequences (each dissolves a LevelSynth hazard):

- **No hashing, no persisted IDs.** Closed, small, declared tagsets get
  **dense indices within their tagset, recomputed at `compile()` from array
  order**. Cells store the index; the index lives only for the run. The
  remap sweep that bit us before *cannot happen* — not because IDs are stable,
  but because **nothing durable stores a tag ID at all** (grids are derived from
  seed + pipeline + registry, never saved; see §4a). Renaming is free, reorder
  is cosmetic, no monotonic counter to keep consistent across undo.
- **No within-value hierarchy in v1.** "Match anything" lives in the pattern
  token (`.`), not in the tag value. Don't re-import the three 21-bit levels.
  A future "match any enemy" is a tagset feature, added when needed.
- **Name collisions are free.** `items[sword]` vs `weapons[sword]` resolve
  because lookup is layer → tagset → value, never global.

**Tagset registry** is a separate object from the layer stack: it owns
name↔color↔glyph for each tag value (the vocabulary). The layer stack owns the
*content*. Both serialize together. Glyphs/colors carry over from MGSL §6 as the
cell-painting palette; the textual glyph alias (`wall █`) is only an
export/interop nicety, since in a visual editor you *paint* the cell.

### 4a. Tag identity & serialization — resolved

The frame to drop is LevelSynth's "renaming must touch zero *cells*." That
guarantee only matters if cells are **durable**. Here they are not: grids are
**derived** (seed + pipeline + registry → grids, deterministically, §7.2). We
save the *recipe*, not the result, and regenerate. So a cell's tag ID exists
only between `compile()` and end-of-run, then is discarded. "Stable across
save/load" is meaningless for a value that never crosses a save.

The clean split: **durable tag storage is by name; transient tag storage is by
index; they meet only at `compile()`.**

- **Durable, by name.** Patterns (in the rewrite transform) store tag
  *references by name* — `geometry.wall`, never an integer. Layer→tagset binding
  is a name (§7). These are few, authored, and human-stable.
- **Transient, by index.** Runtime grids store the resolved **index** into the
  tagset's value vector. Dense (`0..N-1`), so they pack into the smallest int
  holding N — the seam for a future `uint16` tag grid + SIMD, with no compaction
  (indices stay dense; a never-reused monotonic ID would not). Live only for the
  run; never serialized.
- **They meet at `compile()`.** Name references resolve to indices against the
  current registry. **Stop-and-error on an unresolved name** ("rule X references
  geometry.wall, which no longer exists") — loud failure, no silent corruption.

Why no monotonic `next_id` counter (considered and rejected): a counter is
*mutable state that participates in undo* — undo an add, does it roll back? redo?
You'd snapshot it into every history entry and reason about reuse-after-undo. A
plain value vector has none of that: undo restores the vector, indices fall out
of position. The counter was a second source of truth to keep consistent with
the vector across history; deleting it deletes that obligation. (This is §2
subtraction: the counter defended "stable IDs," an invariant the derived-grid
model makes unnecessary.)

What a tagset serializes to — note **no `id`, no `next_id`**:

```jsonc
{
  "name": "geometry",
  "values": [
    { "name": "wall",  "color": "#3a3a3a", "glyph": "U+2588" },
    { "name": "floor", "color": "#c8c8c8", "glyph": "U+00B7" }
  ]
}
```

**The project save is four things, no grid contents anywhere:**

1. **tagset registry** — tagsets as above.
2. **layer declarations** — name, kind, tagset-name (the binding; §7). *Not* cell
   values.
3. **pipeline** — ordered transforms via each transform's `save()` (§6).
4. **seed**.

Load → `evaluate()` → grids appear. Because no durable artifact stores a tag ID,
rename is free and reorder is purely cosmetic.

Two invariants this creates, stated so they don't bite:

- **An edit that changes `compile()` invalidates the timeline (§11).** Timeline
  snapshots hold indices valid only under the registry/pipeline that produced
  them; adding/removing/reordering a tag or editing the pipeline means re-run.
  Fine — the timeline is a debug artifact of one specific run, never serialized.
- **No tagset or pipeline edits during execution.** `compile()` builds the
  name→index map once; all transforms in a run share it. The editor gates this
  naturally (edit, then run). An invariant the engine *assumes*, not one it
  checks mid-loop.

> Practical ceiling: >~100 values in one tagset is already a design smell, so the
> dense-index space is never under pressure; `uint16` covers a project lifetime
> with room to spare even before considering that indices are recomputed, not
> accumulated.

## 5. The grid and the cell — resolved

**One concrete grid. No template, no `tag_grid`/`number_grid` split.** The cell
is `int64`. Tag grids and number grids have *identical storage*; what differs is
*interpretation*, which lives on the layer (§7), not in the grid. (LevelSynth's
design intent was a template on `std::integral`; its source used a concrete
`ls::grid` — the concrete version was right. Carry it.)

"Arithmetic on a tag grid is nonsense" is enforced at the *transform* level (a
numeric transform checks its target layer's kind and refuses a tag layer), not
by a cell type the matcher would have to be generic over.

> **Where "which grid is tag vs number, and which tagset" lives:** *not here.*
> The grid is deliberately ignorant of cell meaning — that's the §4 resolution.
> The binding (`kind` + `tagset` name) sits on the `layer` record in §7. A number
> grid is `kind == number_grid` with an empty `tagset`. The binding is by **name**
> (you can't serialize a pointer), resolved lazily for display/authoring; the
> matcher never needs it (it compares indices). See §4a.

### Empty — via an occupancy bitplane, uniform across all grids

Empty is **not** a sentinel value and **not** ID 0. It is a separate W×H
**occupancy bitplane** carried by every grid, tag and number alike. Reason: the
moment bit operations on cells exist, a sentinel becomes a value some legitimate
op could produce. The bitplane keeps the value plane free to hold any bit
pattern, so bit ops stay clean by construction. (This retracts an earlier
asymmetric idea of "ID 0 = empty on tag grids, bitplane only on number grids" —
it's a bitplane for *both*.)

### `bit_plane`, not `std::vector<bool>`

`vector<bool>` is the one specialization that packs bits, but it isn't a real
container (proxy `operator[]`, no `bool*`, surprising `auto&`), and it gives no
word-level access. We want word-at-a-time ops. Use a small explicit type over
`uint64` words, reused for all bit-shaped things:

```cpp
struct bit_plane {
    int w = 0, h = 0;
    std::vector<uint64_t> words;            // ceil(w*h / 64); value-copy = independent
    void reset(int W, int H);               // size + zero
    bool test(int x, int y) const;
    void set (int x, int y);
    void clear(int x, int y);
    void clear_all();
};
```

The **three bit-shaped things, by lifetime** (keep them distinct):

1. **occupancy** — persistent, one per grid, serialized with the grid.
2. **overlap mask** — transient, per *statement* (rewrite strategy), `clear_all`
   at entry, discarded at exit. Never serialized.
3. **snapshot dirty-set** — optional, only if we ever leave full-copy snapshots.
   Not in v1.

### The grid type

```cpp
struct grid {
    int w = 0, h = 0;
    std::vector<int64_t> values;   // pure, raw, any bit pattern legal
    bit_plane            occupied; // parallel; same w,h; copied & resized TOGETHER

    void    set(int x, int y, int64_t v) { values[y*w+x] = v; occupied.set(x,y); }
    void    clear(int x, int y)          { occupied.clear(x,y); }   // value left stale, unread while empty
    bool    empty(int x, int y) const    { return !occupied.test(x,y); }
    int64_t get(int x, int y) const      { return values[y*w+x]; }  // only valid where !empty
    bool    in_bounds(int x, int y) const;
    // resize / upscale: MUST transform values AND occupied in lockstep, in one method.
};
```

The cost of separating occupancy from value (vs. fusing) is that they are two
parallel arrays that must stay co-sized and be copied/resized together. Make the
lockstep **structural** — only `grid`'s own mutators touch them, never separate
code paths. `upscale` (MGSL §6.2) must duplicate the occupancy bit alongside the
value into each N×M block, or you silently corrupt "present vs empty." `clear`
deliberately does not zero the value — an empty cell's value is dead and the
matcher asks `empty()` first.

## 6. Transforms — the interface

A **transform** is one entry in the pipeline. It reads and writes the layer
stack. There are ~a dozen; they are registered (retargeted `LS_REGISTER`), flat,
no recursion at this level.

**State lives in typed C++ members** (pleasant, type-checked, no string-keyed UI).
JSON appears only at the `save`/`load` boundary, as one block each — never
threaded through every field, never written into by the UI.

```cpp
struct transform {
    virtual json save() const = 0;                              // typed members -> json, at the boundary
    virtual void load(const json&) = 0;                         // json -> typed members, at the boundary
    virtual void compile() {}                                   // OPT-IN precompute (footprints, expansion, name->index)
    virtual void apply(layer_stack&, transform_context&) = 0;   // the work
    virtual void render_ui() = 0;                               // edits typed members directly; the one hand-written UI
    // + LS_REGISTER metadata: display name, category, factory. Flat, ~dozen.
};
```

Why no visitor / no JSON-owned document:

- The visitor was elegant for a *flat bag of scalars* (a LevelSynth node) and
  awful for *recursive structure* (a rewrite rule is a tree). With "fewer but
  more custom" transforms, a generic schema-projector would be overridden by
  every interesting transform — you'd build the engine *and* the custom widgets.
  Premature abstraction (§2).
- Fully JSON-owned state means string-keyed reads/writes in the UI and hot path
  — unfriendly. Typed members + JSON-at-the-boundary keeps editing pleasant and
  serialization free-ish.

The honest cost of typed members is that `save`/`load`/`render_ui` must agree
about the fields (the three-way-drift risk the visitor existed to kill). It's
acceptable here because (a) the bodies genuinely differ per transform, so
collapsing them never fit, and (b) the blast radius is ~a dozen local types, not
a systemic tax. **Discipline that replaces the visitor's structural safety: a
round-trip unit test per transform** — build, `save`, `load` into a fresh
instance, `save` again, assert equal. Three lines; catches a forgotten field
instantly; the §8 unit-test-first habit doing what it's good at.

**`compile()` is opt-in precompute, not a global IR.** `resize`/`upscale` skip
it. The rewrite transform overrides it to derive footprints (MGSL §5.7), expand
symmetry/rotation variants (MGSL §10.3), and resolve grid-name→layer-index. So:
*storage model (typed members) and runtime model (a compiled private struct) are
different things; the gap is zero for trivial transforms and nonzero for the
rewrite transform.* Per-transform, opt-in — not a layer everything pays for.

### Widget helper library (not an engine)

To stop a dozen hand-written panels from reinventing sliders: a thin library of
**widget helpers** (labeled drag-int, enum combo, tag-color cell) the panels
call. Shared spelling, not a framework. `resize`'s panel is two helper calls;
the rewrite painter is genuinely its own thing.

## 7. The layer stack — what `level` stores

Grids are **named, ordered, co-sized, and typed once** (the MGSL `layers`
block). The stack owns the shared size and the ordered layer list; the per-layer
record binds kind + tagset to a grid.

```cpp
enum class layer_kind { tag_grid, number_grid /*, graph — deferred, see §11 */ };

struct layer {
    std::string name;     // "level", "algo", "enemies"
    layer_kind  kind;
    std::string tagset;   // tagset name for tag_grid; empty for number_grid
    grid        data;     // value + occupancy
};

struct layer_stack {
    int w = 0, h = 0;            // shared size; resize/upscale touch ALL layers
    std::vector<layer> layers;   // ordered + named
    layer*       find(std::string_view name);        // LINEAR SCAN — few layers, no index map (§9 of LevelSynth)
    const layer* find(std::string_view name) const;
};
```

Three points, each a prior decision cashing out:

- **Tagset binding on the layer, not the cell** — the §4 resolution in code. Cell
  is a bare index (transient; §4a); layer says which tagset reads it via a tagset
  *name*; the tagset registry (separate object) owns name↔color↔glyph per value.
- **Linear scan, not a name→index map.** Five-ish layers; building the map is the
  exact premature abstraction LevelSynth §9 warns against.
- **The runtime never does name lookup.** A rule references grids by name
  (`algo[F]`); those resolve to **layer indices once, at `compile()`** (heir to
  `rebuild_bindings()`), so `apply()` indexes a vector, not a hash, in the hot
  loop. When the layer set changes in the editor, transforms recompile + rebind.

## 8. Evaluation — engine, context, ownership

**The engine owns lifetime; transforms borrow.** Transforms never `new` a grid,
mask, or snapshot — they ask the context. This routes all allocation through one
place (so the timeline toggle and any future pooling hook there), keeps a
misbehaving transform from leaking state that breaks determinism, and is the
extension point for reuse.

**Simplest borrow that works: plain call-scoped references. No RAII handle, no
pool ticket.** Both are lifetime machinery for a danger a *synchronous linear*
pipeline already excludes — a borrow can't outlive its `apply()` call, nothing
runs concurrently, the reference simply isn't valid after the call. Reuse
buffers across transforms (even that is borderline-premature at 120×80, kept
only because it costs nothing in complexity).

```cpp
struct eval_engine {
    layer_stack    stack;
    std::mt19937_64 rng;          // ONE PRNG for the whole run (per-seed determinism, MGSL §7.2)
    grid           scratch;       // reused
    bit_plane      overlap_mask;  // reused; clear_all() before lending
    layer_stack    snapshot;      // reused for the per-statement snapshot (MGSL §7.1)

    transform_context ctx_for(transform&);   // hands out & to the above
};

struct transform_context {
    std::mt19937_64& rng;
    // resolved name->index bindings the transform compiled against
    // services on request: fresh_overlap_mask(w,h), snapshot(stack)
    // NOT: rewrite-specific machinery baked in. BSP asks for none of it.
};
```

Keep the context **thin**: RNG + bindings + an allocation/scratch surface — what
*all* transforms need. The overlap mask and per-statement snapshot are *rewrite*
concerns; they're services the rewrite transform *requests*, so they don't fatten
the contract every transform sees.

**Two per-statement concerns, different jobs** (don't conflate): the **snapshot**
keeps all matches in a statement reading one consistent input (MGSL §7.1); the
**overlap mask** prevents conflicting *writes* among already-succeeded candidates
(MGSL §6.4). v1 snapshot = full copy (MGSL §10.4); don't optimize until a
profiler says so.

## 9. The editor

A single-window ImGui app (library/editor split preserved). No node canvas.
The two surfaces:

- **Pipeline view** — the ordered transform list (the visual form of MGSL's
  `program` block). Add/remove/reorder transforms; per-transform parameter
  panels via `render_ui()` + the widget helpers (§6).
- **Rule painter** — the rewrite transform's editor. This is the *one* place
  irreducible complexity lives (§10).

Carried-over editor behavior worth keeping: light/dark themes with system-follow;
HiDPI scaling (`SDL_GetWindowDisplayScale`, `ConfigDpiScaleFonts`,
`ScaleAllSizes`, manual scaling of hardcoded pixel constants); New/Open/Save/Save
As + recent files + unsaved-changes modal; atomic `.lsg`-equivalent save
(temp + rename); copy/paste.

### Undo — snapshot-based, granular by structure

Undo = `history.push(t.save())` for the **edited transform** (not the whole
pipeline) — granular *for free*, because state is partitioned per transform; no
delta machinery (the LevelSynth §10 horizon item we explicitly skip). The
LevelSynth pain wasn't snapshot storage, it was **edit-boundary detection** (the
positions-only drag hack, the `activated/deactivated_after_edit/changed`
bracketing tangled into the visitor). Hand-written `render_ui()` fixes this: the
UI *knows* when a paint stroke begins/ends and **declares** "this interaction =
one undo entry" explicitly. Remember **coalescing** — a multi-cell paint pushes
one entry on mouse-up, not one per cell. Undo/redo invalidates the affected
transform's `compile()` cache (cheap; undo isn't hot).

## 10. The rewrite transform — the one earned-complex piece

Everything else got simpler; this got *earned*-complex, and it's irreducible
(painting patterns, the any/all tree, symmetry preview, a tag palette with
any/empty brushes). The design goal here is **containment** — keep its
complexity inside this one transform so it doesn't leak into the engine or the
other eleven.

### `rule` is interior data, NOT a transform subclass

A transform is a stack entry (iterated, registered, `apply`s the layer stack). A
`rule` is one transform's authored content — no `apply`, no registration, no
place in the stack. MGSL mapping: a *statement* (`some(max=5) start`) is the
transform (strategy + params + which rule); the *rule* is the match-write
content it runs. Making `rule` a transform subclass would reintroduce
nestable evaluation structure (sub-rules → transforms-in-transforms) — the
control-flow-graph the fork exists to avoid. So:

```cpp
struct rewrite_transform : transform {
    int           m_max = 100;        // strategy param (one / some(max) / all)
    symmetry_mode m_sym = symmetry_mode::none;
    rotation_mode m_rot = rotation_mode::none;
    rule          m_rule;             // owned interior data — typed tree
    void compile() override;          // footprints + symmetry/rotation expansion + name->index
    void apply(layer_stack&, transform_context&) override;  // runs the strategy/cursor/mask over m_rule
    // save / load / render_ui ...
};

struct rule {                         // recursion lives HERE and only here — data nesting, not eval nesting
    std::vector<pattern>     lhs;
    std::vector<alternative> rhs;     // alternatives carry weights (MGSL §5.5)
    std::vector<rule>        subrules;// MGSL §5.2 nesting
    json to_json() const; static rule from_json(const json&); void render();
};
```

### `pattern` is its own type — NOT a grid

A pattern shares the cell *vocabulary* with grids (so matching is a clean
per-cell compare against the same ID space) but is a richer container. A grid
cell holds one value; a pattern cell holds a match/write **token** (MGSL §2.5,
§5.7):

- `.` — match-anything; in *neither* footprint (matcher never inspects it).
- `?` — match/write the empty cell.
- a tag value or integer literal — in both footprints.

Forcing patterns to *be* grids would smuggle `.` and `?` in as magic sentinel
values — the same sentinel hazard rejected for empty (§5). Put the distinction
in the *type*:

```cpp
enum class cell_kind { any, empty, value };   // '.', '?', concrete value
struct pattern_cell { cell_kind kind; int64_t value = 0; };  // value meaningful iff kind==value
struct pattern {
    int w, h;
    std::vector<pattern_cell> cells;
    // read/write footprints derived from `kind` at compile() — a trivial walk,
    // because the union IS the footprint info, pre-baked into the representation.
};
```

This asymmetry pays off in the UI too: the pattern painter needs an *any* and
*empty* brush alongside the tag-value brushes — affordances that make no sense on
a result-grid preview. Distinct types keep the two editors from contaminating
each other.

## 11. The generation timeline — a free consequence of linearity

The pipeline is linear and you already full-copy snapshot per statement, and the
layer stack is a copyable value type. A timeline is just *keeping* the
after-state of each transform instead of discarding it:

```cpp
struct timeline {
    std::vector<layer_stack> states;   // states[0] = initial; states[i] = after transform i
    // scrubbing = render states[i].  O(1) — no replay, no re-execution.
};
```

Scrubbing is O(1) because linearity means each step has exactly one predecessor —
the affordance a control-flow graph would have *cost*, this model gives for free.
This is a write-up-worthy "architecture yields the debugging tool" moment, and it
directly serves the FDG-survey value of *process transparency*.

**Three state lifetimes, kept separate** (the timeline forces the distinction):

1. **persistent / inter-transform** — the `layer_stack`, snapshotted into the
   timeline *between* transforms.
2. **per-execution / transient** — overlap mask + per-statement snapshot, local
   to `apply()`, **must not** leak into the timeline.
3. **shared / read-mostly** — RNG + bindings, in the context.

**Cost:** memory grows with pipeline length × grid size (trivial for the worked
example, ~few hundred KB). Make timeline capture a **debug toggle** (on while
authoring, off for production batch runs). The deferred delta-snapshot
optimization would shrink it if ever needed. v1: full copies, toggleable, no
optimization.

## 12. Deferred (with the seam already in place)

- **Graph layers** (point 4). The `layer_kind` enum is the seam. Keep `layer.data`
  a plain `grid` until a concrete transform *pair* (generate-mission-graph +
  graph-to-space) forces `data` to become a `grid`-or-`graph` variant. Naming +
  ordering already accommodate a graph layer; storage shouldn't widen yet.
  Graphs are wanted as exportable artifacts (pathfinding, gameplay) — but as a
  **data layer**, never as control flow.
- **Coroutine micro-stepping.** A *future* opt-in `apply_stepped` entry point
  (e.g. `virtual generator<layer_stack> apply_stepped(...)`) on transforms with
  meaningful sub-steps (rewrite, BSP, WFC, CA), engine falling back to plain
  `apply()` otherwise. C++20 coroutines let a transform `co_yield` intermediate
  states while staying written as a natural nested loop. **Hard constraints when
  built:** a yield is a *read-only view* on a deterministic linear unfolding —
  never a branch, never interleaved across transforms (that's control-flow by the
  back door); yielded states are debug-only and must not change results;
  strictly opt-in. It bolts onto *exactly* this architecture (more snapshots on
  the same `states` vector) without touching the context, ownership, or
  timeline — which is the proof it's a safe deferral. **Do not build in v1.**
- **MGSL text format.** The visual model is the source of truth. MGSL
  import/export is optional interop / a paper artifact, possibly dropped
  entirely. The MGSL *front half* (lexer/parser/LSP) is not needed for the tool;
  the *back half* (footprints, expansion, snapshot/cursor/overlap runtime) IS the
  rewrite transform.
- **Tag hierarchy / "match any enemy"** — a future tagset feature, not a reason
  to keep hierarchical cells now.
- **Delta undo, sparse grids, rete-style match indexing** — all MGSL/LevelSynth
  horizon items; wait for a profiler.

## 13. Naming (load-bearing — ends up in the paper)

The **unit** noun is provisionally **transform**, but no single word fits both
the *sweep-the-grid* transforms (rewrite, CA, noise) and the
*construct-from-a-method* ones (BSP, WFC, mission-to-space) — that mismatch is
the same heterogeneity that made the execution contract go thin. Candidates and
their baggage: `operator` (collides with C++/math), `step`/`stage` (too humble,
flattens a BSP into a tutorial instruction), `modifier` (wrong — the generative
ones don't modify), `pass` (good for rewrite, poor for BSP/graph).

Decision posture: **keep `transform` for the unit, define it crisply on first
use** ("one entry in the generation pipeline that reads and writes the shared
layer stack"), and **put the architectural weight on the composition model** —
*pipeline* + *layer stack* + *tagset* — because that triad is the actual claim.
Don't let the unit-noun carry the contribution. **Use the same word in code, UI,
and paper** (LevelSynth's doc drift between "node" and inferred subsystems is the
cautionary tale).

## 14. Suggested build order

1. `bit_plane`, `grid` (value + occupancy, lockstep mutators) + round-trip and
   lockstep unit tests. (§5)
2. Tagset registry (flat, stable IDs, name/color/glyph). (§4)
3. `layer`, `layer_stack` (named, ordered, co-sized; linear `find`). (§7)
4. `transform` base + registry macro retarget; two trivial transforms
   (`resize`, `upscale`) end-to-end with `save`/`load`/`render_ui` + round-trip
   tests. (§6)
5. `eval_engine` + `transform_context` (ownership, call-scoped borrows, one
   PRNG); run a 2-transform pipeline deterministically. (§8)
6. Timeline capture (debug toggle) — nearly free once 5 works. (§11)
7. Rewrite transform: `pattern` / `pattern_cell` / `rule`, `compile()`
   (footprints + symmetry/rotation expansion + name→index), `apply()`
   (snapshot + cursor + overlap mask). Library-level tests against the MGSL
   worked example *semantics*. (§10)
8. Editor: pipeline view, then the rule painter (the earned-complex piece). (§9)

Unit-test the library at every step *without standing up a window* (LevelSynth
§8 posture). The rule painter is last because it's the only piece that can't be
verified headless.
