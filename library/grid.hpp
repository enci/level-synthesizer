#pragma once

#include <cstdint>
#include <vector>

namespace ls {

/// Word-at-a-time bit array. Three distinct lifetimes exist for this type:
///   1. occupancy      — per-grid, serialized, persistent
///   2. overlap mask   — per-statement, transient (clear_all before lending)
///   3. snapshot dirty-set — deferred, not in v1
class bit_plane {
public:
    int w = 0;
    int h = 0;
    std::vector<uint64_t> words; ///< ceil(w*h / 64) packed words

    void reset(int W, int H);
    bool test(int x, int y) const;
    void set(int x, int y);
    void clear(int x, int y);
    void clear_all();

    bool operator==(const bit_plane&) const = default;

private:
    int index(int x, int y) const { return y * w + x; }
};

/// Dense 2D grid of int64 values with a parallel occupancy bitplane.
/// Empty is not a sentinel value — it is tracked by the bitplane.
/// `values` and `occupied` MUST stay co-sized; only this class's own mutators touch them.
class grid {
public:
    int w = 0;
    int h = 0;
    std::vector<int64_t> values;  ///< any bit pattern is legal; only valid where !empty()
    bit_plane             occupied;

    grid() = default;
    grid(int W, int H);

    /// Mark cell occupied and write value. Keeps occupancy and value in lockstep.
    void set(int x, int y, int64_t v);

    /// Mark cell empty. Value is left stale and must not be read until next set().
    void clear(int x, int y);

    bool    empty(int x, int y) const { return !occupied.test(x, y); }
    int64_t get(int x, int y)   const { return values[y * w + x]; }  ///< only valid where !empty
    bool    in_bounds(int x, int y) const;

    /// Resize to new dimensions. Cells within both old and new bounds are preserved
    /// (values AND occupancy, in lockstep). New cells start empty.
    void resize(int new_w, int new_h);

    /// Upscale by integer factors. Each original cell expands into an sx×sy block,
    /// duplicating both value and occupancy. Empty cells expand to empty blocks.
    void upscale(int sx, int sy);

    bool operator==(const grid&) const = default;
};

} // namespace ls
