// v1.20.0 — Fuzzing del gestore delle librerie: JSON rigoroso, library.json,
// index.json, intervalli semver, versioni, nomi e percorsi relativi, piano
// d'installazione (cicli, mancanti, firmware). Primo byte: caso. Invarianti:
// un manifest valido riscritto da manifestJson() si rilegge uguale; un piano
// riuscito ha le dipendenze prima di chi le usa e nessun nome ripetuto.
#include "json_lite.hpp"
#include "lib_manager.hpp"
#include <cstdint>
#include <set>
#include <string>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1 || n > 70000) return 0;
    const std::string t(reinterpret_cast<const char*>(d + 1), n - 1);
    std::string err;
    libs::Version fw;
    (void)libs::Version::parse("1.20.0", fw);
    switch (d[0] % 4) {
    case 0: {
        libs::Manifest m;
        if (libs::parseManifest(t, m, err)) {
            libs::Manifest back;
            std::string e2;
            if (!libs::parseManifest(libs::manifestJson(m), back, e2) || back.name != m.name || !(back.version == m.version) ||
                back.files.size() != m.files.size() || back.dependencies.size() != m.dependencies.size())
                __builtin_trap();
        }
        break;
    }
    case 1: {
        std::vector<libs::IndexEntry> idx;
        if (!libs::parseIndex(t, idx, err)) break;
        for (std::size_t i = 0; i < idx.size() && i < 64; ++i) {
            std::vector<libs::PlanStep> plan;
            if (!libs::makePlan(idx, {}, idx[i].name, idx[i].language, fw, plan, err)) continue;
            std::set<std::string> seen;
            for (const libs::PlanStep& s : plan) {
                if (!seen.insert(s.entry.language + "/" + s.entry.name).second) __builtin_trap();   // due versioni della stessa libreria
                for (const libs::Dependency& dep : s.entry.dependencies) {
                    // ogni dipendenza viene prima (o e' una libreria C per il C++)
                    if (!seen.count(s.entry.language + "/" + dep.name) && !seen.count("c/" + dep.name)) __builtin_trap();
                }
            }
        }
        break;
    }
    case 2: {
        json::Value v;
        (void)json::parse(t, v, err);
        break;
    }
    default: {
        libs::Version v;
        const bool okv = libs::Version::parse(t, v);
        if (okv) { libs::Version w; if (!libs::Version::parse(v.text(), w) || !(w == v)) __builtin_trap(); }
        (void)libs::validRange(t);
        (void)libs::rangeMatches(t, okv ? v : fw);
        if (libs::safeRelativePath(t) && (t.find("..") != std::string::npos && t.find("/../") != std::string::npos)) __builtin_trap();
        if (libs::safeRelativePath(t) && (t[0] == '/' || t.find('\\') != std::string::npos || t.find(':') != std::string::npos)) __builtin_trap();
        (void)libs::validLibName(t);
        break;
    }
    }
    return 0;
}
