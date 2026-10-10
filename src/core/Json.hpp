// Minimal, dependency-free JSON value and parser/serializer.
//
// This module is intentionally independent from Geode and Cocos so that the
// project model, validator and code generator can be unit tested with a plain
// C++ compiler, and so that persisted data does not depend on any single
// third party JSON implementation.
#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace gmb {

// A JSON value. Object keys keep insertion order via a parallel key vector, so
// that serialized files are diff-friendly and deterministic.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() : m_type(Type::Null) {}
    Json(std::nullptr_t) : m_type(Type::Null) {}
    Json(bool v) : m_type(Type::Bool), m_bool(v) {}
    Json(int v) : m_type(Type::Number), m_number(static_cast<double>(v)) {}
    Json(int64_t v) : m_type(Type::Number), m_number(static_cast<double>(v)) {}
    Json(double v) : m_type(Type::Number), m_number(v) {}
    Json(const char* v) : m_type(Type::String), m_string(v) {}
    Json(std::string v) : m_type(Type::String), m_string(std::move(v)) {}

    static Json array() {
        Json j;
        j.m_type = Type::Array;
        return j;
    }
    static Json object() {
        Json j;
        j.m_type = Type::Object;
        return j;
    }

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Type::Null; }
    bool isBool() const { return m_type == Type::Bool; }
    bool isNumber() const { return m_type == Type::Number; }
    bool isString() const { return m_type == Type::String; }
    bool isArray() const { return m_type == Type::Array; }
    bool isObject() const { return m_type == Type::Object; }

    bool asBool(bool def = false) const { return m_type == Type::Bool ? m_bool : def; }
    double asNumber(double def = 0.0) const { return m_type == Type::Number ? m_number : def; }
    int64_t asInt(int64_t def = 0) const { return m_type == Type::Number ? static_cast<int64_t>(m_number) : def; }
    const std::string& asString() const { return m_string; }
    std::string asString(const std::string& def) const { return m_type == Type::String ? m_string : def; }

    // Array access
    const std::vector<Json>& items() const { return m_array; }
    std::vector<Json>& items() { return m_array; }
    size_t size() const {
        if (m_type == Type::Array) return m_array.size();
        if (m_type == Type::Object) return m_keys.size();
        return 0;
    }
    void push(Json v) {
        if (m_type != Type::Array) { m_type = Type::Array; }
        m_array.push_back(std::move(v));
    }

    // Object access
    bool has(const std::string& key) const { return m_index.count(key) != 0; }
    const Json& operator[](const std::string& key) const {
        static const Json null;
        auto it = m_index.find(key);
        return it == m_index.end() ? null : m_array[it->second];
    }
    Json& operator[](const std::string& key) {
        auto it = m_index.find(key);
        if (it != m_index.end()) return m_array[it->second];
        m_type = Type::Object;
        m_keys.push_back(key);
        m_index[key] = m_array.size();
        m_array.emplace_back();
        return m_array.back();
    }
    const std::vector<std::string>& keys() const { return m_keys; }
    void remove(const std::string& key) {
        auto it = m_index.find(key);
        if (it == m_index.end()) return;
        size_t idx = it->second;
        m_array.erase(m_array.begin() + static_cast<long>(idx));
        m_index.erase(key);
        for (auto& entry : m_index) if (entry.second > idx) entry.second -= 1;
        auto kit = std::find(m_keys.begin(), m_keys.end(), key);
        if (kit != m_keys.end()) m_keys.erase(kit);
    }

    // Serialize with pretty printing. Deterministic: object key order is the
    // insertion order preserved above.
    std::string dump(int indent = 2) const;

    // Parse. Returns false and fills `error` on failure.
    static bool parse(std::string_view text, Json& out, std::string& error);

private:
    Type m_type = Type::Null;
    bool m_bool = false;
    double m_number = 0.0;
    std::string m_string;
    std::vector<Json> m_array;      // array items OR object values
    std::vector<std::string> m_keys; // object key order
    std::map<std::string, size_t> m_index;

    void dumpInto(std::string& out, int indent, int depth) const;
};

} // namespace gmb
