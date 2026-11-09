# VPAM — AST condiviso (incremento 1.5.3-dev)

## Contratto

`include/expression_ast.hpp` definisce il modello canonico `fx::ast::Node` e `fx::ast::Kind`.
I nodi sono condivisi tramite `std::shared_ptr` e contengono `kind`, `text` e `children`.
Il contratto è utilizzato da:

- `NaturalEditor`: `NaturalNode` e `NaturalNodeKind` sono alias del tipo canonico;
- `math_renderer`: misura e disegna direttamente lo stesso `fx::ast::Node` ricevuto dall'editor;
- `expression`: `evaluate(const fx::ast::Node&, double)` valuta l'albero senza richiedere al chiamante di produrre una stringa.

## Validazione e serializzazione

`fx::ast::validShape()` controlla l'arità strutturale di ogni tipo di nodo.
`fx::ast::serialize()` converte il sottoinsieme scalare supportato nella sintassi già accettata dal parser testuale. Rifiuta esplicitamente nodi incompleti, pedici e matrici non ancora supportati dal valutatore scalare.

## Limite attuale importante

Questo è il primo passo verso un AST end-to-end, non ancora un parser AST completo: l'input testuale continua a essere analizzato dal parser legacy che valuta durante il parsing. L'overload `evaluate(Node)` serializza l'albero condiviso e delega al parser esistente. La prossima fase deve convertire il parser in `parseExpression()` che costruisce nodi, poi spostare la valutazione su `evaluate(Node)` senza passare dalla serializzazione. Anche selezione, copia/incolla strutturale, undo/redo e AST tipizzati per matrici/vettori/complessi restano da implementare.

## Verifica host

`tests/vpam_ast_smoke.cpp` verifica struttura, serializzazione, valutazione diretta di frazione e potenza, rifiuto di nodi incompleti e compatibilità del nodo radice esposto da `NaturalEditor`.
Eseguire `./tests/run_host_tests.sh` dalla root.

## VPAM AST end-to-end increment (2026-11-07)

This increment moves the arithmetic path from text-only parsing to a typed shared tree:

- `parseExpression()` builds `Binary`, `Unary`, `Call`, `Group`, and `Token` nodes with precedence-aware parsing (`+ - * / % ^`, unary signs, factorial, parentheses, implicit multiplication, and function calls).
- `evaluate(const ast::Node&, x)` now visits the AST directly. It does not call `serialize()`; the scalar visitor currently supports arithmetic, `sqrt`, `abs`, `sin`, `cos`, `tan`, `ln`, `log`, `exp`, `factorial`, and two-argument `pow`.
- `NaturalEditor::setExpression()` imports the parsed tree into the editor's shared root. Existing structural insertion/editing still acts on AST nodes.
- `math::layout()` now measures and draws the new `Binary`, `Unary`, and `Call` nodes, in addition to the existing fraction/radical/power structures.
- `tests/vpam_ast_smoke.cpp` exercises parse -> evaluate -> editor import -> renderer layout, plus invalid-node rejection.

### Deliberate limits of this increment

This is an end-to-end foundation, not completion of every VPAM feature. The legacy string evaluator remains in place for its larger function/memory/calculus catalogue; `evaluate(string)` has not yet been switched to the AST pipeline. AST scalar evaluation does not yet cover matrices, subscripts, calculus nodes, calculator-memory symbols, or the full legacy function catalogue. Editor selection, nested cursor placement when importing a parsed expression, structural clipboard, and undo/redo remain outstanding. The renderer supports the new node forms but does not yet provide full Casio-grade typography or all matrix/calculus layouts.
