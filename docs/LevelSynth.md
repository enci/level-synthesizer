# LevelSynth — Reference

A node-based procedural level generation tool, conceived as "Houdini for level
generation." This document is a baseline description of the project as it
currently stands, intended as a seed for future conversations — including forks
that take the architecture in a different direction.

> **Sourcing note.** This is reconciled from the current source (the
> `node_graph` / `eval_engine` / `generator` library, the editor, and the two
> test suites) and from accumulated project context. Where the two disagree, the
> source wins. A few subsystems (`grid`, `pin`, `tag`, the registries) are
> inferred from their *usage* in tests and the editor rather than from their
> headers, and are flagged where that matters. Treat structural details as
> "confirm against current files when it counts."

---

## 1. Purpose and framing

LevelSynth has two faces. As a dissertation artifact it is framed as a *research
testbed* — not "architecture as the contribution" — that enables
paradigm-comparative study of generation techniques (BSP, WFC,
mission-graph-to-space transforms, and NetHack / Spelunky / Diablo–style
generators), with future technique work layered on top.

As a tool, its target users are design students, and its core values are
designer-friendliness, creative control, process transparency, and accessibility
for non-programmers. These priorities are grounded in the project's companion
survey of 120 game-development professionals (FDG '26), whose central findings
were a statistically significant PCG adoption gap between artists and designers,
and a consistent preference across respondents for creative control and process
transparency over automation. Node-based graphs were noted there as expressive
but significantly underutilized — which is part of why the tool exists.

## 2. Technology

C++20, built with CMake. Runtime/UI stack:

- **SDL3** — windowing, input, renderer, clipboard, preference paths.
- **Dear ImGui** (docking branch, a recent WIP build) — immediate-mode UI.
- **imgui-node-editor** — the canvas, pins, links, selection.
- **FreeType** — font rendering; **Phosphor** icon font for tag/UI glyphs.
- **nlohmann::json** — serialization, kept behind a string-based library API.

The codebase is split into a **library** (`ls::`, no ImGui, no editor concerns)
and an **editor** (ImGui + node-editor, depends on the library). This separation
is a load-bearing design rule, not an accident — see §10.

## 3. Architecture

The library is organized around three collaborating pieces:

**`node_graph`** — pure data: the set of nodes plus the wires between them. It
owns node lifetime, assigns unique incrementing integer IDs, and knows nothing
about evaluation. Observed API: `add_node(unique_ptr<node>) -> int`,
`find_node(id) -> node*`, `node_ids()`, `add_wire({from_node, from_pin, to_node,
to_pin})`, `remove_wire(...)`, `remove_node(id)` (which also drops connected
wires), `wires()`, `clear()`, plus serialization (`save()`, `load(str)`,
`save_subgraph(ids)`, `paste_subgraph(json, dx, dy)`).

**`eval_engine`** — owns evaluation artifacts only. `evaluate(graph, seed)` runs
the graph; `get_output(node_id, pin_name)` returns a pointer to a pin value (a
variant; for grid pins, `std::shared_ptr<grid>`). Evaluation is deterministic in
the seed. Cycles are detected and throw `std::runtime_error`.

**`generator`** — the convenience façade that owns a graph and an engine. It
exposes `graph()`, `engine()`, `set_seed(int)`, `seed()`, `evaluate()`, and a
named-IO surface: `get_grid_output(name)` and `set_parameter(name, double)`,
both of which throw on an unknown name. `rebuild_bindings()` reconnects the named
parameter/output nodes after the graph changes.

Named parameters and outputs are themselves nodes — `node_input_number` carries a
name (e.g. `"density"`) and feeds a value pin; `node_output_grid` /
`node_output_number` carry a name and expose a sink. So `set_parameter("density",
…)` works precisely because some input node is named `"density"`.

### Nodes and the visitor pattern

`node` is the abstract base. Each node declares its pins via `descriptor()`,
implements `evaluate(eval_context&)`, and exposes its persistent fields through
`accept(node_visitor&)`. A node's `accept` calls `node::accept` first (which
visits `name` and `position`) and then visits its own fields:

```cpp
void node_create_grid::accept(node_visitor& v) {
    node::accept(v);
    v.visit("width", m_width);
    v.visit("height", m_height);
    v.visit("fill_value", m_fill_value);
}
```

`node_visitor` has one `visit` overload per persistent type (`double`, `int`,
`vec2`, `std::string`; `bool` is stubbed but commented out). All overloads take
**non-const references**, so the same interface serves readers and writers: the
JSON reader pulls values in and assigns them; the JSON writer reads them out; the
ImGui visitor renders an editing widget for each. Concrete visitors override only
the types they care about. Field names are snake_case; the UI visitor prettifies
them algorithmically.

Visitors currently in the tree: `json_writer` / `json_reader` (in the library),
and `imgui_visitor` (in the editor). The ImGui visitor also reports edit state
(`activated`, `deactivated_after_edit`, `changed`) so the editor can bracket a
property edit with an undo entry and re-evaluate only when something changed.

### Node registry

Nodes self-register via a macro:

```cpp
LS_REGISTER_NODE(node_create_grid, "Create Grid", "Generation");
```

The registry maps a type to a `node_registration` carrying `display_name`,
`category`, `type_name`, and a factory. `instance()` is the singleton;
`find(type_name)`, `find(node&)`, and a static type-based `find` resolve
registrations; `create(type_name)` builds a fresh node; `entries()` enumerates
everything (used to populate the editor's add-node menu, grouped by category).

### The grid type

The runtime grid is a dense 2D array of integer cells (`int64_t` — wide enough to
hold an encoded tag; see §6). Construction is `grid(w, h)` and `grid(w, h, fill)`.
Observed API: `width()`, `height()`, `get(x, y)`, `set(x, y, v)`, `operator()`
(mutable and const), `in_bounds(x, y)`, `fill(v)`, and value-copy semantics
(copies are independent). The grid is *dense-only* for now; a sparse/hybrid
representation is deliberately deferred. Grids flow through the graph individually
as single-layer values.

> Design intent (from project context): the grid is conceived as a template
> constrained on `std::integral`, with the runtime cell type being the 64-bit
> variant. In the current source it is used as a concrete `ls::grid`.

### Pins

A node's `descriptor()` returns a list of `pin_descriptor`s. Each lists a name, a
direction (`input` / `output`), and a type. Current pin types are `number`
(double) and `grid`; `graph` is part of the intended set. Pins also carry a
trailing boolean flag in the descriptor whose exact role is best confirmed from
`pin.hpp` (it varies per pin — e.g. `width`/`height`/`grid` set it, `fill_value`
does not).

### Current node set

- `node_create_grid` — inputs `width`, `height`, `fill_value`; output `grid`.
- `node_noise_grid` — input `grid` and `density`; output `grid` (random 0/1 fill).
- `node_cellular_automata` — input `input`, output `output`; smooths over a number
  of iterations, preserving dimensions.
- `node_input_number` — a named numeric source (`value` output).
- `node_output_grid` / `node_output_number` — named sinks; the output-grid sink is
  what `generator::get_grid_output(name)` reads, and the editor renders a grayscale
  preview on it.

A representative test graph: `Create Grid → Noise Grid → Cellular Automata →
Output Grid`, with a named `density` input feeding the noise node.

## 4. Evaluation contract

`evaluate` walks the graph in dependency order, calling each node's `evaluate`
with an `eval_context` that exposes its resolved inputs (`has_input(name)`,
`input_number(name)`) and accepts its outputs (`set_output_grid(name, …)`). A
node reads from input pins when connected and otherwise falls back to its own
stored field — e.g. `node_create_grid` uses an incoming `width` if wired, else
`m_width`. Determinism is per-seed: same graph + same seed ⇒ identical output;
different seeds ⇒ (with high probability) different output.

## 5. Serialization

Graphs serialize to/from JSON through `node_graph` (`save`/`load`) and to disk as
`.lsg` files, written atomically (temp file + rename). The editor also serializes
**subgraphs** for copy/paste: a selection is dumped to JSON tagged
`"level_synth_clipboard"`, placed on the SDL clipboard, and re-instantiated by
`paste_subgraph`, which returns an old-ID → new-ID map so the editor can reselect
the pasted nodes.

> The two-API split (`add_node` for live construction vs. a load path for
> deserialization) is noted in project context as awkward and slated for a proper
> revisit. It works; it isn't the final shape.

## 6. The tag system

Tags are the cell-semantics layer: a compact value that says *what a cell is*.
They are designed and tested independently of the grid pipeline (see the gap note
in §11).

### Encoding

A tag is a 64-bit value. Bit 63 is a **mode flag**:

- **Symbolic** — the remaining 63 bits hold three equal **21-bit levels**
  (`l0`, `l1`, `l2`). Symbolic tags are built from a dotted path
  (`"Level.Wall.Damaged"`), with each segment hashed (FNV-1a) into its level.
  Hierarchy is *positional* — there are no prescribed role names for the levels.
- **Numeric** — the remaining bits hold a signed 63-bit integer.

API observed in tests: `tag()` (empty — symbolic, all-zero), `tag::numeric(int64)`,
`tag::symbolic(l0, l1, l2)`, plus accessors `type()`, `raw()`, `l0()/l1()/l2()`,
and `value()` (numeric).

### Matching

`match(a, b)` for symbolic tags is **symmetric wildcard** matching: at each level
the tags match if the segments are equal *or* either segment is zero (zero acts
as a wildcard for "any"). So `Wall` (`l1=l2=0`) matches `Wall.Damaged`, and the
relation holds in both directions; two fully-specified siblings like
`Wall.Damaged` and `Wall.Broken` do **not** match (their differing leaf segments
are both non-zero). Mixed-mode (symbolic vs numeric) is always false; two numerics
match iff equal.

> This is what `tests/test_tags.cpp` currently asserts — both `match(wall,
> wall_dmg)` and `match(wall_dmg, wall)` are expected true. Tag matching semantics
> have been revised before, so if a fork leans on tags, re-confirm the exact rule
> against the tests rather than against any remembered description.

### Tag registry

A registry of declared tags, backed by an ordered string-keyed map (lexicographic
for stable UI ordering), with hierarchy derived from the dotted path strings —
the flat dotted string is the single source of truth, not a stored tree.

Tested surface: `add(path) -> bool` (re-adding is a no-op that returns true;
structural garbage like `""`, `"."`, `"Wall."`, `".Wall"`, `"a..b"`, or
over-deep `"a.b.c.d"` returns false), `find(path) -> optional`, `parse(path) ->
optional<tag>` (independent of registration — parsing always succeeds for
well-formed paths), `identifier(tag) -> string`, `valid(tag) -> bool` (numerics
always valid; the empty tag and unregistered symbolic tags are invalid), and
`remove(path)`.

Beyond what the tests exercise, the registry also provides the authoring surface
the tag panel needs: renaming a segment, reparenting, and per-entry color/icon
overrides (colors are generated with optional overrides; icons reference font
glyph identifiers). A deprecated-entries mechanism supports rename workflows
without forcing an immediate propagation sweep.

> Caveat that bit us before: hash-based IDs do **not** make renames free. Changing
> any segment string changes its hash, which means any grid storing that tag needs
> a remap sweep. The deprecated list eases the workflow but doesn't remove the
> sweep.

## 7. The editor

A single-window ImGui application (`application` owns SDL/ImGui lifetime; `editor`
owns the document and UI). Notable behavior:

- **Layout** — a full-viewport dockspace; the node editor, Details panel, and
  History panel dock into it. Layout is rebuilt on first run or after a reset.
- **Node canvas** — nodes drawn via the node-builder with a category-colored
  header, input pins on the left, output pins on the right, and a grayscale grid
  preview rendered on sink nodes. IDs for nodes/pins/links are packed into the
  node-editor's id space with tag bits. New links are accepted only when pin types
  match. Right-click opens an add-node menu grouped by registry category.
- **History / undo-redo** — currently **snapshot-based**: each edit pushes a
  `{description, before_json, after_json}` entry (full-graph JSON). Node drags are
  detected via a positions-only snapshot on press/release to avoid false positives
  from overlapping property edits. The History panel lists entries with sizes and a
  saved-position marker.
- **Details panel** — for a single selected node, runs the `imgui_visitor` to
  render its fields and integrates with the undo/eval cycle.
- **Files** — New / Open / Save / Save As, recent-files list, and an
  unsaved-changes modal gating destructive actions. Copy / cut / paste via the
  clipboard subgraph format.
- **Theming** — hand-tuned light and dark themes (ImGui colors + node-editor
  style), with a system-follow mode that reacts to OS theme changes.
- **HiDPI** — UI scale from `SDL_GetWindowDisplayScale`. Fonts load at natural
  size with `io.ConfigDpiScaleFonts = true`; `style.ScaleAllSizes` is applied
  after each theme apply. The node-editor's own `ed::Style`, hardcoded pixel
  constants, and preview cell sizes need manual scaling. Display-change events are
  handled.

## 8. Testing

Tests use Catch2 and exercise the **library directly**, not the editor UI. Two
suites: `test_library.cpp` (grid, node_graph, eval_engine, generator — including
determinism, cycle detection, and named parameter/output behavior) and
`test_tags.cpp` (tag encoding, matching, and the registry). This unit-test-first
posture is deliberate: the library is verifiable without standing up a window.

## 9. Design principles

These are the recurring decisions the project keeps making, and they're worth
preserving in any fork:

- **Simplicity over premature abstraction.** Collapse abstractions that don't earn
  their keep: a `vector<string>` over a fixed `array<string_view, 3>`; one
  hierarchical tag registry over multiple alphabets; a linear scan over a second
  index map; dense grids before any sparse/hybrid scheme; no runtime plugins.
- **Single source of truth.** Flat dotted strings in the tag registry; hierarchy
  derived per-frame, never stored.
- **Library / editor separation.** `nlohmann::json` lives behind a string-based
  API on `node_graph`; ImGui stays entirely out of the library via a parallel
  editor-side UI registry.
- **Two-pass mutation.** Validate all segments for collisions first, then commit —
  rather than committing-then-rolling-back.
- **Start simple, extend later.** Ship the minimal version; widen scope only when a
  concrete need appears.
- **Verify in unit tests, not the UI.**

## 10. Known gaps and on the horizon

- **Tag ↔ grid integration.** The tag system is built and tested in isolation; the
  runtime pipeline currently fills grids with small plain integers (0/1). Wiring
  encoded tags through generation as cell values is the bridge not yet present in
  the current source — and it's exactly the seam any pattern/rewrite-based fork
  would build on.
- **Mission-graph-to-space transform** — a planned technique contribution.
- **Expression / scripting engine** — expression-only (no statements or control
  flow), compiled to a stack-based bytecode VM for per-tile evaluation; variables
  are read-only lookups from input pins. Parser estimated ~400–600 lines, bytecode
  layer ~100–150.
- **Serialization rework** — replace the awkward two-API construct/load split.
- **Delta-based undo/redo** — replace full-graph snapshots with deltas, preserving
  partial cache invalidation. (The current editor is snapshot-based.)
- **Sparse grids** — deferred behind dense-only.

## 11. Notes for forks

This document is meant to be forkable. The pieces most likely to carry over intact
are the **tag system** (encoding + registry), the **visitor pattern** for node
fields, the **registry-by-macro** approach, the **grid** type, the
**library/editor split**, and the determinism/seed and unit-test conventions. The
pieces most coupled to the node-graph model — pins, wires, topological ordering,
and cycle detection — are the ones a different evaluation model (e.g. an ordered
stack of named layers with sequenced transforms) would replace rather than reuse.
