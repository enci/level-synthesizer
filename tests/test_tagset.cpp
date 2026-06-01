#include <catch2/catch_test_macros.hpp>
#include "tagset.hpp"

using namespace ls;

TEST_CASE("tagset: add returns the runtime index starting from zero", "[tagset]") {
    tagset ts{"geometry", {}};
    REQUIRE(ts.add("wall")  == 0);
    REQUIRE(ts.add("floor") == 1);
    REQUIRE(ts.add("door")  == 2);
}

TEST_CASE("tagset: add rejects duplicate value name", "[tagset]") {
    tagset ts{"geometry", {}};
    ts.add("wall");
    REQUIRE_FALSE(ts.add("wall").has_value());
    REQUIRE(ts.values.size() == 1);
}

TEST_CASE("tagset: find by name returns correct value", "[tagset]") {
    tagset ts{"items", {}};
    ts.add("chest", 0xFF0000FF, 0x1F4E6);
    ts.add("sword");

    auto* v = ts.find("chest");
    REQUIRE(v != nullptr);
    REQUIRE(v->color == 0xFF0000FF);
    REQUIRE(v->glyph == 0x1F4E6);

    REQUIRE(ts.find("sword")  != nullptr);
    REQUIRE(ts.find("potion") == nullptr);
}

TEST_CASE("tagset: find by index is O(1) lookup", "[tagset]") {
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

TEST_CASE("tagset: index equals position in values vector", "[tagset]") {
    tagset ts{"enemies", {}};
    for (auto name : {"goblin", "troll", "dragon", "lich"})
        ts.add(name);

    for (int i = 0; i < static_cast<int>(ts.values.size()); ++i)
        REQUIRE(ts.find(i)->name == ts.values[i].name);
}

TEST_CASE("tagset: default color and glyph applied when not specified", "[tagset]") {
    tagset ts{"geometry", {}};
    ts.add("wall");
    REQUIRE(ts.find("wall")->color == 0x888888FF);
    REQUIRE(ts.find("wall")->glyph == 0);
}

TEST_CASE("tagset: JSON round-trip preserves names, colors, glyphs — no id field", "[tagset]") {
    tagset ts{"geometry", {}};
    ts.add("wall",  0xFF0000FF, 0x2588);
    ts.add("floor", 0x00FF00FF, 0x00B7);

    auto j = ts.to_json();

    REQUIRE_FALSE(j["values"][0].contains("id"));
    REQUIRE(j["values"][0]["name"]  == "wall");
    REQUIRE(j["values"][0]["color"] == "#ff0000ff");
    REQUIRE(j["values"][0]["glyph"] == "U+2588");

    auto restored = tagset::from_json(j);
    REQUIRE(restored.name == ts.name);
    REQUIRE(restored.values.size() == ts.values.size());
    for (size_t i = 0; i < ts.values.size(); ++i) {
        REQUIRE(restored.values[i].name  == ts.values[i].name);
        REQUIRE(restored.values[i].color == ts.values[i].color);
        REQUIRE(restored.values[i].glyph == ts.values[i].glyph);
    }
}

TEST_CASE("tagset: glyph zero serializes as empty string", "[tagset]") {
    tagset ts{"geometry", {}};
    ts.add("wall");
    auto j = ts.to_json();
    REQUIRE(j["values"][0]["glyph"] == "");
}

TEST_CASE("tagset_registry: add creates a new empty tagset", "[tagset_registry]") {
    tagset_registry reg;
    REQUIRE(reg.add("geometry"));
    REQUIRE(reg.find("geometry") != nullptr);
    REQUIRE(reg.find("geometry")->values.empty());
}

TEST_CASE("tagset_registry: add rejects duplicate tagset name", "[tagset_registry]") {
    tagset_registry reg;
    reg.add("geometry");
    REQUIRE_FALSE(reg.add("geometry"));
    REQUIRE(reg.tagsets.size() == 1);
}

TEST_CASE("tagset_registry: same value name in different tagsets is fine", "[tagset_registry]") {
    tagset_registry reg;
    reg.add("geometry");
    reg.add("items");

    auto geo_idx   = reg.find("geometry")->add("sword");
    auto items_idx = reg.find("items")->add("sword");

    REQUIRE(geo_idx.has_value());
    REQUIRE(items_idx.has_value());
    REQUIRE(geo_idx == items_idx);  // both index 0 — independent per-tagset
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

TEST_CASE("tagset_registry: JSON round-trip preserves all tagsets and values", "[tagset_registry]") {
    tagset_registry reg;
    reg.add("geometry");
    reg.find("geometry")->add("wall",  0xFF0000FF, 0x2588);
    reg.find("geometry")->add("floor", 0x00FF00FF, 0x00B7);
    reg.add("items");
    reg.find("items")->add("chest", 0xFFD700FF, 0);

    auto restored = tagset_registry::from_json(reg.to_json());

    REQUIRE(restored.tagsets.size() == 2);
    auto* rg = restored.find("geometry");
    REQUIRE(rg->find("wall")->glyph  == 0x2588);
    REQUIRE(rg->find("floor")->color == 0x00FF00FF);
    REQUIRE(restored.find("items")->find("chest")->color == 0xFFD700FF);
}
