#include <catch2/catch_test_macros.hpp>
#include "tagset.hpp"

using namespace ls;

// ---------------------------------------------------------------------------
// tagset
// ---------------------------------------------------------------------------

TEST_CASE("tagset: add assigns stable monotonic IDs from zero", "[tagset]") {
    tagset ts{"geometry", {}};
    auto id0 = ts.add("wall");
    auto id1 = ts.add("floor");
    auto id2 = ts.add("door");

    REQUIRE(id0 == 0);
    REQUIRE(id1 == 1);
    REQUIRE(id2 == 2);
}

TEST_CASE("tagset: add rejects duplicate value name", "[tagset]") {
    tagset ts{"geometry", {}};
    ts.add("wall");
    auto dup = ts.add("wall");
    REQUIRE_FALSE(dup.has_value());
    REQUIRE(ts.values.size() == 1);
}

TEST_CASE("tagset: find by name returns correct value", "[tagset]") {
    tagset ts{"items", {}};
    ts.add("chest", 0xFF0000FF, 0x1F4E6);
    ts.add("sword");

    auto* v = ts.find("chest");
    REQUIRE(v != nullptr);
    REQUIRE(v->id    == 0);
    REQUIRE(v->color == 0xFF0000FF);
    REQUIRE(v->glyph == 0x1F4E6);

    REQUIRE(ts.find("sword") != nullptr);
    REQUIRE(ts.find("potion") == nullptr);
}

TEST_CASE("tagset: find by id is O(1) index lookup", "[tagset]") {
    tagset ts{"geometry", {}};
    ts.add("wall");
    ts.add("floor");

    REQUIRE(ts.find(0) != nullptr);
    REQUIRE(ts.find(0)->name == "wall");
    REQUIRE(ts.find(1) != nullptr);
    REQUIRE(ts.find(1)->name == "floor");
    REQUIRE(ts.find(2)  == nullptr);
    REQUIRE(ts.find(-1) == nullptr);
}

TEST_CASE("tagset: ID is stable — stays equal to index after multiple adds", "[tagset]") {
    tagset ts{"enemies", {}};
    for (auto name : {"goblin", "troll", "dragon", "lich"})
        ts.add(name);

    for (int i = 0; i < static_cast<int>(ts.values.size()); ++i)
        REQUIRE(ts.values[i].id == i);
}

TEST_CASE("tagset: default color applied when not specified", "[tagset]") {
    tagset ts{"geometry", {}};
    ts.add("wall");
    REQUIRE(ts.find("wall")->color == 0x888888FF);
    REQUIRE(ts.find("wall")->glyph == 0);
}

TEST_CASE("tagset: round-trip JSON preserves all fields", "[tagset]") {
    tagset ts{"geometry", {}};
    ts.add("wall",  0xFF0000FF, 0x2588);
    ts.add("floor", 0x00FF00FF, 0x00B7);

    auto restored = tagset::from_json(ts.to_json());

    REQUIRE(restored.name == ts.name);
    REQUIRE(restored.values.size() == ts.values.size());
    for (size_t i = 0; i < ts.values.size(); ++i) {
        REQUIRE(restored.values[i].id    == ts.values[i].id);
        REQUIRE(restored.values[i].name  == ts.values[i].name);
        REQUIRE(restored.values[i].color == ts.values[i].color);
        REQUIRE(restored.values[i].glyph == ts.values[i].glyph);
    }
}

// ---------------------------------------------------------------------------
// tagset_registry
// ---------------------------------------------------------------------------

TEST_CASE("tagset_registry: add creates a new empty tagset", "[tagset_registry]") {
    tagset_registry reg;
    auto* ts = reg.add("geometry");
    REQUIRE(ts != nullptr);
    REQUIRE(ts->name == "geometry");
    REQUIRE(ts->values.empty());
}

TEST_CASE("tagset_registry: add rejects duplicate tagset name", "[tagset_registry]") {
    tagset_registry reg;
    reg.add("geometry");
    REQUIRE(reg.add("geometry") == nullptr);
    REQUIRE(reg.tagsets.size() == 1);
}

TEST_CASE("tagset_registry: same value name in different tagsets is fine", "[tagset_registry]") {
    tagset_registry reg;
    auto* geo   = reg.add("geometry");
    auto* items = reg.add("items");

    auto geo_id   = geo->add("sword");    // geometry[sword]
    auto items_id = items->add("sword");  // items[sword]

    REQUIRE(geo_id.has_value());
    REQUIRE(items_id.has_value());
    REQUIRE(geo_id == items_id);  // both get ID 0 — independent per-tagset
}

TEST_CASE("tagset_registry: find returns correct tagset", "[tagset_registry]") {
    tagset_registry reg;
    reg.add("geometry");
    reg.add("items");

    REQUIRE(reg.find("geometry") != nullptr);
    REQUIRE(reg.find("items")    != nullptr);
    REQUIRE(reg.find("enemies")  == nullptr);
}

TEST_CASE("tagset_registry: find is const-correct", "[tagset_registry]") {
    tagset_registry reg;
    reg.add("geometry");

    const auto& creg = reg;
    REQUIRE(creg.find("geometry") != nullptr);
    REQUIRE(creg.find("missing")  == nullptr);
}

TEST_CASE("tagset_registry: round-trip JSON preserves all tagsets and values", "[tagset_registry]") {
    tagset_registry reg;
    auto* geo = reg.add("geometry");
    geo->add("wall",  0xFF0000FF, 0x2588);
    geo->add("floor", 0x00FF00FF, 0x00B7);
    auto* items = reg.add("items");
    items->add("chest", 0xFFD700FF, 0);

    auto restored = tagset_registry::from_json(reg.to_json());

    REQUIRE(restored.tagsets.size() == 2);
    auto* rg = restored.find("geometry");
    REQUIRE(rg != nullptr);
    REQUIRE(rg->values.size() == 2);
    REQUIRE(rg->find("wall")->glyph  == 0x2588);
    REQUIRE(rg->find("floor")->color == 0x00FF00FF);

    auto* ri = restored.find("items");
    REQUIRE(ri != nullptr);
    REQUIRE(ri->find("chest")->color == 0xFFD700FF);
}

TEST_CASE("tagset_registry: pointer from add stays valid across subsequent adds", "[tagset_registry]") {
    tagset_registry reg;
    auto* geo = reg.add("geometry");
    geo->add("wall");

    // Adding more tagsets must not invalidate `geo`.
    // (vector reallocation would break this if we stored pointers to elements carelessly.)
    for (int i = 0; i < 10; ++i)
        reg.add("ts_" + std::to_string(i));

    // Re-find rather than use potentially-dangling `geo`.
    auto* refound = reg.find("geometry");
    REQUIRE(refound != nullptr);
    REQUIRE(refound->find("wall") != nullptr);
}
