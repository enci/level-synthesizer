#include "tagset.hpp"

#include <algorithm>

namespace ls {

std::optional<int32_t> tagset::add(std::string_view value_name,
                                   uint32_t color,
                                   uint32_t glyph) {
    if (find(value_name))
        return std::nullopt;
    int32_t id = static_cast<int32_t>(values.size());
    values.push_back({id, std::string(value_name), color, glyph});
    return id;
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
        arr.push_back({{"id", v.id}, {"name", v.name},
                       {"color", v.color}, {"glyph", v.glyph}});
    return {{"name", name}, {"values", arr}};
}

tagset tagset::from_json(const nlohmann::json& j) {
    tagset ts;
    ts.name = j.at("name").get<std::string>();
    for (auto& jv : j.at("values")) {
        tagset_value v;
        v.id    = jv.at("id").get<int32_t>();
        v.name  = jv.at("name").get<std::string>();
        v.color = jv.at("color").get<uint32_t>();
        v.glyph = jv.at("glyph").get<uint32_t>();
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
