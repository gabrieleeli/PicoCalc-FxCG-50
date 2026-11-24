#pragma once
// Nucleo della modalita' Distribution (v1.18.0, modalita' Base; solo Pimoroni
// Pico Plus 2 W): lo stato della modalita' (funzione scelta, parametri di ogni
// funzione, Data Variable/List, liste, Save Res, ultimo risultato) con la
// codifica della sezione 0x0117 del record v12. I calcoli sono quelli di
// dist_core.hpp, lo stesso nucleo del DIST di Statistics.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::dmode {
constexpr std::size_t kKinds = 24;                    // dist::KindCount
struct State {
    std::uint8_t kind{1};                             // indice di dist::kindAt (iniziale: Normal C.D)
    std::array<std::vector<double>, kKinds> params{}; // vuoto = valori iniziali della funzione
    std::uint8_t tail{0};                             // Inverse Normal: Left, Right, Central
    bool listData{false};                             // Data: Variable / List
    std::uint8_t list1{0}, list2{1};                  // liste dei dati (indici 0..25)
    std::uint8_t saveRes{0};                          // 0 None, k = List k
    bool hasResult{false};
    double lastResult{0};
    bool operator==(const State& o) const noexcept;
};
void reset(State& s);
// Parametri della funzione k (i valori iniziali di dist::paramSpecs se mai cambiati).
std::vector<double> paramsOf(const State& s, std::size_t kind);
// "" se valido, altrimenti il motivo.
std::string validate(const State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
// Solo dati validi (numeri finiti, indici nei limiti, numero di parametri giusto); s invariato se false.
bool decode(State& s, const std::uint8_t* data, std::size_t size);
} // namespace fx::dmode
