# Level Synthesizer — Claude context

A visual procedural level-generation tool: a linear pipeline of heterogeneous
transforms over a shared typed layer stack (blackboard model). Designed as the
"Fork Architecture" successor to the node-graph LevelSynth.

Design docs live in `docs/`:
- `Level Synthesizer Architecture.md` — the fork architecture and build order
- `LevelSynth.md` — original LevelSynth reference
- `mgsl-spec.md` — MGSL spec, doubles as the rewrite-transform specification

## Build & test

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
ctest --output-on-failure
```

Requires CMake ≥ 3.20, a C++20 compiler. Catch2 and nlohmann_json are fetched
automatically via FetchContent.

## Project structure

```
library/         synth_library static library (no ImGui, no editor concerns)
tests/           Catch2 unit tests, one file per layer
docs/            Design documents
```

## Code style

- **Documentation comments**: use `///` (C# style) on all public types, fields,
  and methods in headers. `//` is for non-doc inline notes only.
- **No divider comments**: `// ---` separator lines are not used. Structure
  comes from namespaces, types, and blank lines.
- **No comments that restate the name**: only add a `///` comment when the WHY
  or a non-obvious invariant needs stating.
- **Namespace**: `ls`

## Build order (Architecture doc §14)

1. ✅ `bit_plane`, `grid` — `library/grid.hpp/.cpp`
2. Tagset registry — flat, stable IDs, name/color/glyph
3. `layer`, `layer_stack` — named, ordered, co-sized
4. `transform` base + registry + `resize` / `upscale` transforms
5. `eval_engine` + `transform_context`
6. Timeline capture (debug toggle)
7. Rewrite transform — pattern/rule, compile, apply
8. Editor — pipeline view, then rule painter
