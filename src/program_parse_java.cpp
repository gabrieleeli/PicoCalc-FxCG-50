// Parser di Java (v1.21.0). Nella v1.20.0 il dialetto Java non e' ancora
// disponibile: un file .java da' un errore chiaro.
#include "program_ast.hpp"

namespace fx::prog::ast {

void parseJava(const Lexed& lexed, Unit& unit, ParseContext& ctx) {
    (void)ctx;
    (void)unit;
    Pos at;
    if (!lexed.tokens.empty()) at = lexed.tokens.front().pos;
    fail("Java non e' ancora disponibile (arriva nella v1.21.0)", at);
}

} // namespace fx::prog::ast
