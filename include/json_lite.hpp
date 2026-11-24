#pragma once
// JSON minimo e rigoroso (v1.20.0, solo Pimoroni Pico Plus 2 W) per i
// manifest delle librerie (library.json) e l'indice del repository
// (index.json): RFC 8259 senza estensioni (niente commenti ne' virgole
// finali), con limiti di dimensione, profondita' e numero di nodi; un testo
// malformato da' un errore chiaro con riga e colonna, mai un crash
// (tests/fuzz/fuzz_libjson.cpp).
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fx::json {

struct Limits {
    std::size_t maxBytes{64 * 1024};
    int maxDepth{16};
    std::size_t maxNodes{4096};
    std::size_t maxString{8 * 1024};
};

class Value {
public:
    enum class Type : std::uint8_t { Null, Bool, Number, String, Array, Object };
    Type type{Type::Null};
    bool b{false};
    double n{0};
    std::string s;
    std::vector<Value> items;                            // Array
    std::vector<std::pair<std::string, Value>> fields;   // Object (ordine del testo, chiavi uniche)
    bool isNull() const noexcept { return type == Type::Null; }
    bool isString() const noexcept { return type == Type::String; }
    bool isNumber() const noexcept { return type == Type::Number; }
    bool isObject() const noexcept { return type == Type::Object; }
    bool isArray() const noexcept { return type == Type::Array; }
    bool isBool() const noexcept { return type == Type::Bool; }
    const Value* get(const std::string& key) const noexcept;   // Object: campo o nullptr
};

// false con `error` ("riga 3, colonna 7: ':' atteso").
bool parse(const std::string& text, Value& out, std::string& error, const Limits& limits = Limits{});
// Testo JSON compatto (stringhe con gli escape necessari).
std::string quote(const std::string& s);

} // namespace fx::json
