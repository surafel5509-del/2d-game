#pragma once
// ============================================================
//  Nebula Engine - json/json.hpp
//  Dependency-free JSON value, parser and serializer used for
//  scene files (.nebula.json), project manifests and saves.
// ============================================================
#include <string>
#include <vector>
#include <utility>

namespace nebula {
namespace json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() : type_(Type::Null) {}
    Value(bool b) : type_(Type::Bool), bool_(b) {}
    Value(int i) : type_(Type::Number), num_(static_cast<double>(i)) {}
    Value(double d) : type_(Type::Number), num_(d) {}
    Value(int64_t i) : type_(Type::Number), num_(static_cast<double>(i)) {}
    Value(const char* s) : type_(Type::String), str_(s ? s : "") {}
    Value(const std::string& s) : type_(Type::String), str_(s) {}
    Value(const std::vector<Value>& a) : type_(Type::Array), arr_(a) {}

    static Value array()  { Value v; v.type_ = Type::Array;  return v; }
    static Value object() { Value v; v.type_ = Type::Object; return v; }

    Type type() const { return type_; }
    bool isNull()   const { return type_ == Type::Null; }
    bool isBool()   const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray()  const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool(bool fallback = false) const { return isBool() ? bool_ : fallback; }
    double asNumber(double fallback = 0.0) const { return isNumber() ? num_ : fallback; }
    float  asFloat(float fallback = 0.0f) const { return static_cast<float>(asNumber(fallback)); }
    int    asInt(int fallback = 0) const { return static_cast<int>(asNumber(fallback)); }
    const std::string& asString() const { return str_; }
    const std::vector<Value>& asArray() const { return arr_; }

    // Object access (const). Returns nullptr when missing.
    const Value* find(const std::string& key) const {
        if (!isObject()) return nullptr;
        for (const auto& kv : obj_) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    const Value& at(const std::string& key) const {
        const Value* v = find(key);
        static const Value kNull;
        return v ? *v : kNull;
    }

    // Object/array mutation.
    Value& operator[](const std::string& key) {
        if (!isObject()) { type_ = Type::Object; obj_.clear(); }
        for (auto& kv : obj_) if (kv.first == key) return kv.second;
        obj_.emplace_back(key, Value());
        return obj_.back().second;
    }
    const std::vector<std::pair<std::string, Value>>& members() const { return obj_; }

    void push(const Value& v) {
        if (!isArray()) { type_ = Type::Array; arr_.clear(); }
        arr_.push_back(v);
    }
    size_t size() const { return isArray() ? arr_.size() : obj_.size(); }

    std::string dump(int indent = 2) const;

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Value> arr_;
    std::vector<std::pair<std::string, Value>> obj_;
};

// Throws std::runtime_error on malformed input.
Value parse(const std::string& text);

} // namespace json
} // namespace nebula
