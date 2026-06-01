#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace ls {

/// One named value within a tagset. ID is a stable monotonic integer
/// assigned at insertion time and never changes, even on rename.
struct tagset_value {
    int32_t     id    = 0;
    std::string name;
    uint32_t    color = 0x888888FF; ///< RGBA packed, editor display hint
    uint32_t    glyph = 0;          ///< Unicode codepoint; 0 = no glyph
};

/// A closed, flat set of named values. Cells in a tag-layer store the integer
/// ID and interpret it through the tagset bound to that layer.
/// IDs are 0-based indices into `values` — find(id) is O(1).
class tagset {
public:
    std::string               name;
    std::vector<tagset_value> values;

    /// Add a new value. Returns the assigned ID, or empty if the name already exists.
    std::optional<int32_t> add(std::string_view value_name,
                               uint32_t color = 0x888888FF,
                               uint32_t glyph = 0);

    const tagset_value* find(std::string_view value_name) const;
    const tagset_value* find(int32_t id) const;

    nlohmann::json to_json() const;
    static tagset  from_json(const nlohmann::json&);
};

/// Owns all tagsets for a document. The layer stack references tagsets by name;
/// name→tagset resolution happens once at compile(), not in hot loops.
class tagset_registry {
public:
    std::vector<tagset> tagsets;

    /// Add a new empty tagset. Returns false if the name is already taken.
    /// Callers must use find() to populate the tagset after adding.
    bool add(std::string_view tagset_name);

    tagset*       find(std::string_view tagset_name);
    const tagset* find(std::string_view tagset_name) const;

    nlohmann::json to_json() const;
    static tagset_registry from_json(const nlohmann::json&);
};

} // namespace ls
