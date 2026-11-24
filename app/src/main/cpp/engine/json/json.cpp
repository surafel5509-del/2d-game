// ============================================================
//  Nebula Engine - json/json.cpp
//  Hand-rolled, allocation-conscious JSON parser + serializer.
//  Supports: null, true/false, numbers, strings (with escapes
//  and \uXXXX), arrays, objects, nesting. No comments (strict).
// ============================================================
#include "engine/json/json.hpp"
#include <cctype>
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <sstream>

namespace nebula {
namespace json {

namespace {

struct Parser {
    const char* cur;
    const char* end;

    void skipWs() {
        while (cur < end && (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r')) ++cur;
    }

    [[noreturn]] void fail(const char* msg) { throw std::runtime_error(std::string("JSON parse error: ") + msg); }

    Value parseValue() {
        skipWs();
        if (cur >= end) fail("unexpected end of input");
        switch (*cur) {
            case '{': return parseObject();
            case '[': return parseArray();
            case '"': return Value(parseString());
            case 't': expect("true");  return Value(true);
            case 'f': expect("false"); return Value(false);
            case 'n': expect("null");  return Value();
            default:  return parseNumber();
        }
    }

    void expect(const char* literal) {
        while (*literal) {
            if (cur >= end || *cur != *literal) fail("bad literal");
            ++cur; ++literal;
        }
    }

    Value parseObject() {
        ++cur; // '{'
        Value obj = Value::object();
        skipWs();
        if (cur < end && *cur == '}') { ++cur; return obj; }
        while (true) {
            skipWs();
            if (cur >= end || *cur != '"') fail("expected object key");
            std::string key = parseString();
            skipWs();
            if (cur >= end || *cur != ':') fail("expected ':'");
            ++cur;
            Value val = parseValue();
            obj[key] = std::move(val);
            skipWs();
            if (cur < end && *cur == ',') { ++cur; continue; }
            if (cur < end && *cur == '}') { ++cur; break; }
            fail("expected ',' or '}'");
        }
        return obj;
    }

    Value parseArray() {
        ++cur; // '['
        Value arr = Value::array();
        skipWs();
        if (cur < end && *cur == ']') { ++cur; return arr; }
        while (true) {
            arr.push(parseValue());
            skipWs();
            if (cur < end && *cur == ',') { ++cur; continue; }
            if (cur < end && *cur == ']') { ++cur; break; }
            fail("expected ',' or ']'");
        }
        return arr;
    }

    std::string parseString() {
        ++cur; // '"'
        std::string out;
        while (cur < end) {
            char c = *cur++;
            if (c == '"') return out;
            if (c == '\\') {
                if (cur >= end) break;
                char esc = *cur++;
                switch (esc) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u': {
                        if (end - cur < 4) fail("bad \\u escape");
                        int cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = *cur++;
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp += h - '0';
                            else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
                            else fail("bad \\u escape");
                        }
                        // UTF-8 encode (BMP only; surrogate pairs best-effort).
                        if (cp < 0x80) out += static_cast<char>(cp);
                        else if (cp < 0x800) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: fail("bad escape");
                }
            } else {
                out += c;
            }
        }
        fail("unterminated string");
    }

    Value parseNumber() {
        const char* start = cur;
        if (cur < end && (*cur == '-' || *cur == '+')) ++cur;
        bool any = false;
        while (cur < end && (std::isdigit((unsigned char)*cur) || *cur == '.' ||
                             *cur == 'e' || *cur == 'E' || *cur == '-' || *cur == '+')) {
            ++cur; any = true;
        }
        if (!any) fail("expected value");
        try {
            return Value(std::stod(std::string(start, cur)));
        } catch (...) {
            fail("bad number");
        }
    }
};

void dumpString(const std::string& s, std::string& out) {
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

void dumpValue(const Value& v, std::string& out, int indent, int depth) {
    const std::string pad(indent > 0 ? static_cast<size_t>(indent) * (depth + 1) : 0, ' ');
    const std::string padEnd(indent > 0 ? static_cast<size_t>(indent) * depth : 0, ' ');
    const char* nl = indent > 0 ? "\n" : "";

    switch (v.type()) {
        case Value::Type::Null:   out += "null"; break;
        case Value::Type::Bool:   out += v.asBool() ? "true" : "false"; break;
        case Value::Type::Number: {
            double d = v.asNumber();
            char buf[32];
            if (std::fabs(d - std::llround(d)) < 1e-9 && std::fabs(d) < 9e15) {
                std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(std::llround(d)));
            } else {
                std::snprintf(buf, sizeof(buf), "%.6g", d);
            }
            out += buf;
            break;
        }
        case Value::Type::String: dumpString(v.asString(), out); break;
        case Value::Type::Array: {
            const auto& arr = v.asArray();
            out += '[';
            for (size_t i = 0; i < arr.size(); ++i) {
                if (indent > 0 && i > 0) {}
                out += nl; out += pad;
                dumpValue(arr[i], out, indent, depth + 1);
                if (i + 1 < arr.size()) out += ',';
            }
            if (!arr.empty()) { out += nl; out += padEnd; }
            out += ']';
            break;
        }
        case Value::Type::Object: {
            const auto& members = v.members();
            out += '{';
            for (size_t i = 0; i < members.size(); ++i) {
                out += nl; out += pad;
                dumpString(members[i].first, out);
                out += indent > 0 ? ": " : ":";
                dumpValue(members[i].second, out, indent, depth + 1);
                if (i + 1 < members.size()) out += ',';
            }
            if (!members.empty()) { out += nl; out += padEnd; }
            out += '}';
            break;
        }
    }
}

} // namespace

std::string Value::dump(int indent) const {
    std::string out;
    dumpValue(*this, out, indent, 0);
    return out;
}

Value parse(const std::string& text) {
    Parser p{text.c_str(), text.c_str() + text.size()};
    Value v = p.parseValue();
    p.skipWs();
    return v;
}

} // namespace json
} // namespace nebula
