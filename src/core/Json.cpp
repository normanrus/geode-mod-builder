#include "core/Json.hpp"

#include <cctype>
#include <cstdio>
#include <cmath>

namespace gmb {

static void escapeString(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

static void numberToString(std::string& out, double v) {
    if (std::isfinite(v) && v == static_cast<double>(static_cast<int64_t>(v))) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        out += buf;
    } else if (std::isfinite(v)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.10g", v);
        out += buf;
    } else {
        out += "0";
    }
}

void Json::dumpInto(std::string& out, int indent, int depth) const {
    switch (m_type) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += (m_bool ? "true" : "false"); break;
        case Type::Number: numberToString(out, m_number); break;
        case Type::String: escapeString(out, m_string); break;
        case Type::Array: {
            if (m_array.empty()) { out += "[]"; break; }
            out.push_back('[');
            bool first = true;
            for (const auto& item : m_array) {
                if (!first) out.push_back(',');
                first = false;
                if (indent > 0) { out.push_back('\n'); out.append(static_cast<size_t>(indent) * (depth + 1), ' '); }
                item.dumpInto(out, indent, depth + 1);
            }
            if (indent > 0) { out.push_back('\n'); out.append(static_cast<size_t>(indent) * depth, ' '); }
            out.push_back(']');
            break;
        }
        case Type::Object: {
            if (m_keys.empty()) { out += "{}"; break; }
            out.push_back('{');
            bool first = true;
            for (const auto& key : m_keys) {
                if (!first) out.push_back(',');
                first = false;
                if (indent > 0) { out.push_back('\n'); out.append(static_cast<size_t>(indent) * (depth + 1), ' '); }
                escapeString(out, key);
                out.push_back(':');
                if (indent > 0) out.push_back(' ');
                (*this)[key].dumpInto(out, indent, depth + 1);
            }
            if (indent > 0) { out.push_back('\n'); out.append(static_cast<size_t>(indent) * depth, ' '); }
            out.push_back('}');
            break;
        }
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    dumpInto(out, indent, 0);
    return out;
}

namespace {

struct Parser {
    std::string_view s;
    size_t i = 0;
    std::string error;

    bool fail(const std::string& msg) {
        if (error.empty()) {
            error = msg + " at offset " + std::to_string(i);
        }
        return false;
    }
    void skipWs() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
    }
    bool parseValue(Json& out) {
        skipWs();
        if (i >= s.size()) return fail("unexpected end of input");
        char c = s[i];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') { std::string str; if (!parseString(str)) return false; out = Json(std::move(str)); return true; }
        if (c == 't') return parseLiteral("true", Json(true), out);
        if (c == 'f') return parseLiteral("false", Json(false), out);
        if (c == 'n') return parseLiteral("null", Json(nullptr), out);
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(out);
        return fail("unexpected character");
    }
    bool parseLiteral(std::string_view lit, Json value, Json& out) {
        if (s.substr(i, lit.size()) != lit) return fail("invalid literal");
        i += lit.size();
        out = value;
        return true;
    }
    bool parseNumber(Json& out) {
        size_t start = i;
        if (i < s.size() && s[i] == '-') i++;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
        if (i < s.size() && s[i] == '.') { i++; while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++; }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            i++;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) i++;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
        }
        try {
            out = Json(std::stod(std::string(s.substr(start, i - start))));
        } catch (...) {
            return fail("invalid number");
        }
        return true;
    }
    bool parseString(std::string& out) {
        if (s[i] != '"') return fail("expected string");
        i++;
        out.clear();
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= s.size()) return fail("bad escape");
                char e = s[i++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (i + 4 > s.size()) return fail("bad unicode escape");
                        unsigned cp = 0;
                        for (int k = 0; k < 4; k++) {
                            char h = s[i++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                            else return fail("bad hex");
                        }
                        // Encode as UTF-8 (BMP only; surrogate pairs are rare in
                        // UI text and treated literally).
                        if (cp < 0x80) out.push_back(static_cast<char>(cp));
                        else if (cp < 0x800) {
                            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        }
                        break;
                    }
                    default: return fail("unknown escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return fail("unterminated string");
    }
    bool parseArray(Json& out) {
        out = Json::array();
        i++; // [
        skipWs();
        if (i < s.size() && s[i] == ']') { i++; return true; }
        while (true) {
            Json v;
            if (!parseValue(v)) return false;
            out.push(std::move(v));
            skipWs();
            if (i >= s.size()) return fail("unterminated array");
            if (s[i] == ',') { i++; continue; }
            if (s[i] == ']') { i++; return true; }
            return fail("expected , or ]");
        }
    }
    bool parseObject(Json& out) {
        out = Json::object();
        i++; // {
        skipWs();
        if (i < s.size() && s[i] == '}') { i++; return true; }
        while (true) {
            skipWs();
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (i >= s.size() || s[i] != ':') return fail("expected :");
            i++;
            Json v;
            if (!parseValue(v)) return false;
            out[key] = std::move(v);
            skipWs();
            if (i >= s.size()) return fail("unterminated object");
            if (s[i] == ',') { i++; continue; }
            if (s[i] == '}') { i++; return true; }
            return fail("expected , or }");
        }
    }
};

} // namespace

bool Json::parse(std::string_view text, Json& out, std::string& error) {
    Parser p{text, 0, {}};
    if (!p.parseValue(out)) {
        error = p.error;
        return false;
    }
    p.skipWs();
    if (p.i != p.s.size()) {
        error = "trailing data at offset " + std::to_string(p.i);
        return false;
    }
    return true;
}

} // namespace gmb
