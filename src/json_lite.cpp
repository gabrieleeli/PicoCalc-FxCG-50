// JSON minimo e rigoroso: vedi json_lite.hpp.
#include "json_lite.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace fx::json {

const Value* Value::get(const std::string& key) const noexcept {
    if (type != Type::Object) return nullptr;
    for (const auto& f : fields) if (f.first == key) return &f.second;
    return nullptr;
}

namespace {
class JsonParser {
public:
    JsonParser(const std::string& t, const Limits& l, std::string& err) : t_(t), l_(l), err_(err) {}
    bool run(Value& out) {
        if (t_.size() > l_.maxBytes) return fail("testo troppo grande (" + std::to_string(t_.size()) + " byte, massimo " + std::to_string(l_.maxBytes) + ")");
        // BOM UTF-8 iniziale ammesso
        if (t_.size() >= 3 && static_cast<unsigned char>(t_[0]) == 0xEF && static_cast<unsigned char>(t_[1]) == 0xBB && static_cast<unsigned char>(t_[2]) == 0xBF) p_ = 3;
        ws();
        if (!value(out, 0)) return false;
        ws();
        if (p_ != t_.size()) return fail("testo dopo la fine del valore");
        return true;
    }
private:
    bool fail(const std::string& m) {
        std::size_t line = 1, col = 1;
        for (std::size_t i = 0; i < p_ && i < t_.size(); ++i) { if (t_[i] == '\n') { ++line; col = 1; } else ++col; }
        err_ = "riga " + std::to_string(line) + ", colonna " + std::to_string(col) + ": " + m;
        return false;
    }
    void ws() { while (p_ < t_.size() && (t_[p_] == ' ' || t_[p_] == '\t' || t_[p_] == '\n' || t_[p_] == '\r')) ++p_; }
    bool lit(const char* w) {
        const std::size_t n = std::strlen(w);
        if (t_.compare(p_, n, w) != 0) return false;
        p_ += n;
        return true;
    }
    bool value(Value& v, int depth) {
        if (depth > l_.maxDepth) return fail("annidamento troppo profondo");
        if (++nodes_ > l_.maxNodes) return fail("troppi elementi");
        if (p_ >= t_.size()) return fail("valore atteso");
        const char c = t_[p_];
        if (c == '{') return object(v, depth);
        if (c == '[') return array(v, depth);
        if (c == '"') { v.type = Value::Type::String; return string(v.s); }
        if (c == 't') { if (!lit("true")) return fail("valore non valido"); v.type = Value::Type::Bool; v.b = true; return true; }
        if (c == 'f') { if (!lit("false")) return fail("valore non valido"); v.type = Value::Type::Bool; v.b = false; return true; }
        if (c == 'n') { if (!lit("null")) return fail("valore non valido"); v.type = Value::Type::Null; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) return number(v);
        return fail(std::string("carattere inatteso '") + (static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 127 ? std::string(1, c) : std::string("?")) + "'");
    }
    bool number(Value& v) {
        const std::size_t b = p_;
        if (t_[p_] == '-') ++p_;
        if (p_ >= t_.size()) return fail("numero incompleto");
        if (t_[p_] == '0') ++p_;
        else if (t_[p_] >= '1' && t_[p_] <= '9') { while (p_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[p_]))) ++p_; }
        else return fail("numero non valido");
        if (p_ < t_.size() && t_[p_] == '.') {
            ++p_;
            if (p_ >= t_.size() || !std::isdigit(static_cast<unsigned char>(t_[p_]))) return fail("cifre attese dopo il punto");
            while (p_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[p_]))) ++p_;
        }
        if (p_ < t_.size() && (t_[p_] == 'e' || t_[p_] == 'E')) {
            ++p_;
            if (p_ < t_.size() && (t_[p_] == '+' || t_[p_] == '-')) ++p_;
            if (p_ >= t_.size() || !std::isdigit(static_cast<unsigned char>(t_[p_]))) return fail("esponente non valido");
            while (p_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[p_]))) ++p_;
        }
        if (p_ - b > 64) return fail("numero troppo lungo");
        const std::string text = t_.substr(b, p_ - b);
        v.type = Value::Type::Number;
        v.n = std::strtod(text.c_str(), nullptr);
        if (!std::isfinite(v.n)) return fail("numero fuori campo");
        return true;
    }
    static void put(std::string& s, std::uint32_t cp) {
        if (cp < 0x80) s += static_cast<char>(cp);
        else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    bool hex4(std::uint32_t& out) {
        out = 0;
        if (p_ + 4 > t_.size()) return fail("escape \\u incompleto");
        for (int i = 0; i < 4; ++i) {
            const char c = t_[p_++];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return fail("escape \\u non valido");
        }
        return true;
    }
    bool string(std::string& out) {
        ++p_;                                                   // "
        out.clear();
        for (;;) {
            if (p_ >= t_.size()) return fail("stringa senza '\"' finale");
            const unsigned char c = static_cast<unsigned char>(t_[p_]);
            if (c == '"') { ++p_; break; }
            if (c < 0x20) return fail("carattere di controllo nella stringa");
            if (c == '\\') {
                ++p_;
                if (p_ >= t_.size()) return fail("escape incompleto");
                const char e = t_[p_++];
                switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        std::uint32_t lo = 0;
                        if (p_ + 2 > t_.size() || t_[p_] != '\\' || t_[p_ + 1] != 'u') return fail("coppia surrogata incompleta");
                        p_ += 2;
                        if (!hex4(lo)) return false;
                        if (lo < 0xDC00 || lo > 0xDFFF) return fail("coppia surrogata non valida");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) return fail("surrogato isolato");
                    put(out, cp);
                    break;
                }
                default: return fail("escape non valido");
                }
            } else {
                // UTF-8 valido
                std::size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
                if (len == 0 || p_ + len > t_.size()) return fail("UTF-8 non valido");
                for (std::size_t k = 1; k < len; ++k) if ((static_cast<unsigned char>(t_[p_ + k]) & 0xC0) != 0x80) return fail("UTF-8 non valido");
                out.append(t_, p_, len);
                p_ += len;
            }
            if (out.size() > l_.maxString) return fail("stringa troppo lunga");
        }
        return true;
    }
    bool array(Value& v, int depth) {
        ++p_;
        v.type = Value::Type::Array;
        ws();
        if (p_ < t_.size() && t_[p_] == ']') { ++p_; return true; }
        for (;;) {
            ws();
            Value item;
            if (!value(item, depth + 1)) return false;
            v.items.push_back(std::move(item));
            ws();
            if (p_ >= t_.size()) return fail("']' attesa");
            if (t_[p_] == ',') { ++p_; continue; }
            if (t_[p_] == ']') { ++p_; return true; }
            return fail("',' o ']' attesa");
        }
    }
    bool object(Value& v, int depth) {
        ++p_;
        v.type = Value::Type::Object;
        ws();
        if (p_ < t_.size() && t_[p_] == '}') { ++p_; return true; }
        for (;;) {
            ws();
            if (p_ >= t_.size() || t_[p_] != '"') return fail("nome del campo atteso");
            std::string key;
            if (!string(key)) return false;
            for (const auto& f : v.fields) if (f.first == key) return fail("campo '" + key + "' ripetuto");
            ws();
            if (p_ >= t_.size() || t_[p_] != ':') return fail("':' atteso");
            ++p_;
            ws();
            Value item;
            if (!value(item, depth + 1)) return false;
            v.fields.emplace_back(std::move(key), std::move(item));
            ws();
            if (p_ >= t_.size()) return fail("'}' attesa");
            if (t_[p_] == ',') { ++p_; continue; }
            if (t_[p_] == '}') { ++p_; return true; }
            return fail("',' o '}' attesa");
        }
    }
    const std::string& t_;
    const Limits& l_;
    std::string& err_;
    std::size_t p_{0};
    std::size_t nodes_{0};
};
} // namespace

bool parse(const std::string& text, Value& out, std::string& error, const Limits& limits) {
    out = Value();
    JsonParser p(text, limits, error);
    if (!p.run(out)) { out = Value(); return false; }
    return true;
}

std::string quote(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else o += static_cast<char>(c);
        }
    }
    return o + "\"";
}

} // namespace fx::json
