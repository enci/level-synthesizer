#include <catch2/catch_test_macros.hpp>
#include "grid.hpp"

using namespace ls;

// ---------------------------------------------------------------------------
// bit_plane tests
// ---------------------------------------------------------------------------

TEST_CASE("bit_plane: default constructed is empty", "[bit_plane]") {
    bit_plane bp;
    REQUIRE(bp.w == 0);
    REQUIRE(bp.h == 0);
    REQUIRE(bp.words.empty());
}

TEST_CASE("bit_plane: reset sizes word array correctly", "[bit_plane]") {
    bit_plane bp;
    bp.reset(8, 8);   // 64 bits — exactly one word
    REQUIRE(bp.words.size() == 1);
    bp.reset(9, 8);   // 72 bits — two words
    REQUIRE(bp.words.size() == 2);
    bp.reset(1, 1);
    REQUIRE(bp.words.size() == 1);
}

TEST_CASE("bit_plane: all bits start clear after reset", "[bit_plane]") {
    bit_plane bp;
    bp.reset(5, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            REQUIRE_FALSE(bp.test(x, y));
}

TEST_CASE("bit_plane: set and test round-trip", "[bit_plane]") {
    bit_plane bp;
    bp.reset(4, 4);
    bp.set(1, 2);
    REQUIRE(bp.test(1, 2));
    REQUIRE_FALSE(bp.test(0, 0));
    REQUIRE_FALSE(bp.test(3, 3));
}

TEST_CASE("bit_plane: clear resets a set bit", "[bit_plane]") {
    bit_plane bp;
    bp.reset(4, 4);
    bp.set(2, 3);
    REQUIRE(bp.test(2, 3));
    bp.clear(2, 3);
    REQUIRE_FALSE(bp.test(2, 3));
}

TEST_CASE("bit_plane: clear_all resets all bits", "[bit_plane]") {
    bit_plane bp;
    bp.reset(5, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            bp.set(x, y);
    bp.clear_all();
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            REQUIRE_FALSE(bp.test(x, y));
}

TEST_CASE("bit_plane: bits across word boundary are independent", "[bit_plane]") {
    // Word boundary is at bit 64. Use a 65x1 grid: cells 63 and 64 straddle the boundary.
    bit_plane bp;
    bp.reset(65, 1);
    REQUIRE(bp.words.size() == 2);

    bp.set(63, 0);   // last bit of word 0
    bp.set(64, 0);   // first bit of word 1

    REQUIRE(bp.test(63, 0));
    REQUIRE(bp.test(64, 0));

    bp.clear(63, 0);
    REQUIRE_FALSE(bp.test(63, 0));
    REQUIRE(bp.test(64, 0));

    bp.clear(64, 0);
    REQUIRE_FALSE(bp.test(64, 0));
}

TEST_CASE("bit_plane: bit 63 and 64 are in separate words", "[bit_plane]") {
    bit_plane bp;
    bp.reset(64, 1);  // exactly one word
    REQUIRE(bp.words.size() == 1);
    bp.set(63, 0);
    REQUIRE(bp.test(63, 0));

    bp.reset(65, 1);  // two words
    REQUIRE(bp.words.size() == 2);
    bp.set(63, 0);
    bp.set(64, 0);
    REQUIRE(bp.test(63, 0));
    REQUIRE(bp.test(64, 0));
    REQUIRE_FALSE(bp.test(62, 0));
}

TEST_CASE("bit_plane: equality", "[bit_plane]") {
    bit_plane a, b;
    a.reset(3, 3);
    b.reset(3, 3);
    REQUIRE(a == b);
    a.set(1, 1);
    REQUIRE_FALSE(a == b);
    b.set(1, 1);
    REQUIRE(a == b);
}

// ---------------------------------------------------------------------------
// grid tests
// ---------------------------------------------------------------------------

TEST_CASE("grid: default constructed has zero size", "[grid]") {
    grid g;
    REQUIRE(g.w == 0);
    REQUIRE(g.h == 0);
    REQUIRE(g.values.empty());
}

TEST_CASE("grid: sized construction — all cells empty", "[grid]") {
    grid g(4, 3);
    REQUIRE(g.w == 4);
    REQUIRE(g.h == 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 4; ++x)
            REQUIRE(g.empty(x, y));
}

TEST_CASE("grid: set marks occupied and stores value", "[grid]") {
    grid g(5, 5);
    g.set(2, 3, 42);
    REQUIRE_FALSE(g.empty(2, 3));
    REQUIRE(g.get(2, 3) == 42);
}

TEST_CASE("grid: clear marks empty without changing other cells", "[grid]") {
    grid g(5, 5);
    g.set(1, 1, 7);
    g.set(2, 2, 9);
    g.clear(1, 1);
    REQUIRE(g.empty(1, 1));
    REQUIRE_FALSE(g.empty(2, 2));
    REQUIRE(g.get(2, 2) == 9);
}

TEST_CASE("grid: set overwrites an existing occupied cell", "[grid]") {
    grid g(3, 3);
    g.set(0, 0, 1);
    g.set(0, 0, 99);
    REQUIRE(g.get(0, 0) == 99);
    REQUIRE_FALSE(g.empty(0, 0));
}

TEST_CASE("grid: in_bounds rejects negative and out-of-range", "[grid]") {
    grid g(4, 3);
    REQUIRE(g.in_bounds(0, 0));
    REQUIRE(g.in_bounds(3, 2));
    REQUIRE_FALSE(g.in_bounds(-1, 0));
    REQUIRE_FALSE(g.in_bounds(0, -1));
    REQUIRE_FALSE(g.in_bounds(4, 0));
    REQUIRE_FALSE(g.in_bounds(0, 3));
}

TEST_CASE("grid: negative values are stored correctly", "[grid]") {
    grid g(2, 2);
    g.set(0, 0, -1);
    g.set(1, 1, INT64_MIN);
    REQUIRE(g.get(0, 0) == -1);
    REQUIRE(g.get(1, 1) == INT64_MIN);
}

TEST_CASE("grid: equality", "[grid]") {
    grid a(3, 3), b(3, 3);
    REQUIRE(a == b);
    a.set(1, 1, 5);
    REQUIRE_FALSE(a == b);
    b.set(1, 1, 5);
    REQUIRE(a == b);
}

// ---------------------------------------------------------------------------
// Lockstep invariant: resize
// ---------------------------------------------------------------------------

TEST_CASE("grid: resize preserves cells in overlapping region", "[grid][resize]") {
    grid g(4, 4);
    g.set(0, 0, 1);
    g.set(3, 3, 2);

    g.resize(6, 6);
    REQUIRE(g.w == 6);
    REQUIRE(g.h == 6);
    REQUIRE_FALSE(g.empty(0, 0));
    REQUIRE(g.get(0, 0) == 1);
    REQUIRE_FALSE(g.empty(3, 3));
    REQUIRE(g.get(3, 3) == 2);
    // New cells are empty
    REQUIRE(g.empty(5, 5));
    REQUIRE(g.empty(4, 0));
}

TEST_CASE("grid: resize crops cells outside new bounds", "[grid][resize]") {
    grid g(5, 5);
    g.set(4, 4, 99);
    g.set(0, 0, 1);

    g.resize(3, 3);
    REQUIRE(g.w == 3);
    REQUIRE(g.h == 3);
    REQUIRE_FALSE(g.empty(0, 0));
    REQUIRE(g.get(0, 0) == 1);
    // (4,4) no longer exists; (2,2) is within bounds and should be empty
    REQUIRE(g.empty(2, 2));
}

TEST_CASE("grid: resize keeps occupancy and values in lockstep", "[grid][resize]") {
    grid g(3, 3);
    g.set(1, 1, 42);
    g.resize(5, 5);
    // Occupancy must mirror value: cell (1,1) occupied, others empty
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            if (x == 1 && y == 1) {
                REQUIRE_FALSE(g.empty(x, y));
                REQUIRE(g.get(x, y) == 42);
            } else {
                REQUIRE(g.empty(x, y));
            }
}

TEST_CASE("grid: resize to same size is a no-op", "[grid][resize]") {
    grid g(3, 3);
    g.set(0, 0, 7);
    grid before = g;
    g.resize(3, 3);
    REQUIRE(g == before);
}

TEST_CASE("grid: resize empty cell does not bleed occupancy into new grid", "[grid][resize]") {
    grid g(3, 3);
    // deliberately do NOT set (1,1) — it should remain empty after resize
    g.resize(5, 5);
    REQUIRE(g.empty(1, 1));
}

// ---------------------------------------------------------------------------
// Lockstep invariant: upscale
// ---------------------------------------------------------------------------

TEST_CASE("grid: upscale 2x2 expands dimensions", "[grid][upscale]") {
    grid g(2, 3);
    g.upscale(2, 2);
    REQUIRE(g.w == 4);
    REQUIRE(g.h == 6);
}

TEST_CASE("grid: upscale occupied cell fills the entire block", "[grid][upscale]") {
    grid g(2, 2);
    g.set(0, 0, 5);
    g.upscale(3, 2);

    REQUIRE(g.w == 6);
    REQUIRE(g.h == 4);
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 3; ++dx) {
            REQUIRE_FALSE(g.empty(dx, dy));
            REQUIRE(g.get(dx, dy) == 5);
        }
}

TEST_CASE("grid: upscale empty cell expands to empty block", "[grid][upscale]") {
    grid g(2, 2);
    g.set(0, 0, 1);
    // (1,1) is empty
    g.upscale(2, 2);

    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx)
            REQUIRE(g.empty(2 + dx, 2 + dy));
}

TEST_CASE("grid: upscale keeps occupancy and values in lockstep", "[grid][upscale]") {
    grid g(2, 2);
    g.set(0, 0, 7);
    g.set(1, 1, 3);
    g.upscale(2, 2);

    // (0,0) block: cells (0,0),(1,0),(0,1),(1,1)
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            REQUIRE_FALSE(g.empty(dx, dy));
            REQUIRE(g.get(dx, dy) == 7);
        }
    // (1,1) block: cells (2,2),(3,2),(2,3),(3,3)
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            REQUIRE_FALSE(g.empty(2 + dx, 2 + dy));
            REQUIRE(g.get(2 + dx, 2 + dy) == 3);
        }
    // (1,0) block and (0,1) block: empty
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            REQUIRE(g.empty(2 + dx, dy));
            REQUIRE(g.empty(dx, 2 + dy));
        }
}

TEST_CASE("grid: upscale 1x1 is identity", "[grid][upscale]") {
    grid g(3, 3);
    g.set(1, 2, 100);
    grid before = g;
    g.upscale(1, 1);
    REQUIRE(g == before);
}
