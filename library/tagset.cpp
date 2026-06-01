#include "tagset.hpp"

#include <algorithm>
#include <cstdio>

namespace ls {

namespace {
    std::string color_to_string(uint32_t color) {
        char buf[10];
        std::snprintf(buf, sizeof(buf), "#%08x", color);
        return buf;
    }

    uint32_t color_from_string(std::string_view s) {
        if (s.size() == 9 && s[0] == '#')
            return static_cast<uint32_t>(std::stoul(std::string(s.substr(1)), nullptr, 16));
        return 0x888888FF;
    }

    std::string glyph_to_string(uint32_t glyph) {
        if (glyph == 0) return "";
        char buf[12];
        std::snprintf(buf, sizeof(buf), "U+%04X", glyph);
        return buf;
    }

    uint32_t glyph_from_string(std::string_view s) {
        if (s.size() > 2 && s[0] == 'U' && s[1] == '+')
            return static_cast<uint32_t>(std::stoul(std::string(s.substr(2)), nullptr, 16));
        return 0;
    }
}

std::optional<int32_t> tagset::add(std::string_view value_name,
                                   uint32_t color,
                                   uint32_t glyph) {
    if (find(value_name))
        return std::nullopt;
    int32_t index = static_cast<int32_t>(values.size());
    values.push_back({std::string(value_name), color, glyph});
    return index;
}

const tagset_value* tagset::find(std::string_view value_name) const {
    for (auto& v : values)
        if (v.name == value_name)
            return &v;
    return nullptr;
}

const tagset_value* tagset::find(int32_t id) const {
    if (id < 0 || id >= static_cast<int32_t>(values.size()))
        return nullptr;
    return &values[id];
}

nlohmann::json tagset::to_json() const {
    nlohmann::json arr = nlohmann::json::array();
    for (auto& v : values)
        arr.push_back({{"name",  v.name},
                       {"color", color_to_string(v.color)},
                       {"glyph", glyph_to_string(v.glyph)}});
    return {{"name", name}, {"values", arr}};
}

tagset tagset::from_json(const nlohmann::json& j) {
    tagset ts;
    ts.name = j.at("name").get<std::string>();
    for (auto& jv : j.at("values")) {
        tagset_value v;
        v.name  = jv.at("name").get<std::string>();
        v.color = color_from_string(jv.at("color").get<std::string>());
        v.glyph = glyph_from_string(jv.at("glyph").get<std::string>());
        ts.values.push_back(v);
    }
    return ts;
}

bool tagset_registry::add(std::string_view tagset_name) {
    if (find(tagset_name))
        return false;
    tagsets.push_back({std::string(tagset_name), {}});
    return true;
}

tagset* tagset_registry::find(std::string_view tagset_name) {
    for (auto& ts : tagsets)
        if (ts.name == tagset_name)
            return &ts;
    return nullptr;
}

const tagset* tagset_registry::find(std::string_view tagset_name) const {
    for (auto& ts : tagsets)
        if (ts.name == tagset_name)
            return &ts;
    return nullptr;
}

nlohmann::json tagset_registry::to_json() const {
    nlohmann::json arr = nlohmann::json::array();
    for (auto& ts : tagsets)
        arr.push_back(ts.to_json());
    return {{"tagsets", arr}};
}

tagset_registry tagset_registry::from_json(const nlohmann::json& j) {
    tagset_registry reg;
    for (auto& jts : j.at("tagsets"))
        reg.tagsets.push_back(tagset::from_json(jts));
    return reg;
}

} // namespace ls
