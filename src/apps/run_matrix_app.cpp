// RUN-MATRIX: input Math (2D) o Line, cronologia modificabile sul posto con
// ricalcolo (funzione History della fx-CG50), risultati esatti con S<->D,
// frazioni miste e decimali periodici, matrici, vettori e liste (editor
// MAT/VCT: matrix_editor.cpp), catalogo OPTN, VARS (A-Z e le variabili
// condivise con Graph/Table), freccia di memorizzazione, viste Base-N e DMS,
// selezione CLIP (SHIFT 8 / SHIFT 9), parentesi colorate e cursore sulla
// causa di un errore.
#include "apps.hpp"
#include "calc_library.hpp"
#include "graph_commands.hpp"
#include "math_renderer.hpp"
#include "physical_constants.hpp"
#include "run_matrix.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

namespace {
// Menu di Run-Matrix (v1.12.0): ogni voce e' un'etichetta e un'azione.
//   sub:NOME   apre un sottomenu          sym:T  inserisce il token T
//   fn:T       inserisce T e "("           txt:T  inserisce il testo T
//   tpl:NOME   modello Math                view:K vista del risultato (S⇔D, ab/c, periodo, basi, DMS)
//   eng:±1     ENG / ENG←                  cview:1/0  ►r∠θ / ►a+bi
//   unit:U     unita' di CONVERT           var:X  variabile A-Z (in Hex: v_x per A-F)
//   sketch:C   nome di un comando grafico  fm*:n  FUNCMEM            capture:n  CAPTURE
struct MenuItem { std::string label; std::string action; };
using MenuList = std::vector<MenuItem>;
// OPTN di Run-Matrix nell'ordine della fx-CG50 (LIST, MAT/VCT, COMPLEX, CALC, STAT,
// CONVERT, HYPERBL, PROB, NUMERIC, ANGLE, ESYM, PICTURE, FUNCMEM, LOGIC, CAPTURE,
// TVM), poi i menu che la calcolatrice ha su altri tasti (SKETCH = SHIFT F4,
// relazioni = PRGM, viste del risultato, Base-N = tasti F del modo Base-N).
MenuList optnRoot() {
    return {{"LIST  List Dim Seq Mean", "sub:LIST"}, {"MAT/VCT  Mat Det Trn Vct", "sub:MAT"}, {"COMPLEX  i Arg Conjg \xE2\x88\xA0", "sub:CPLX"},
            {"CALC  Solve d/dx \xE2\x88\xAB SolveN FMin", "sub:CALC"}, {"STAT  \xEE\x80\x80 \xC5\xB7 DIST S-Dev", "sub:STAT"},
            {"CONVERT  [cm]\xE2\x96\xB6[in]", "sub:CONV"}, {"HYPERBL  sinh cosh tanh", "sub:HYP"}, {"PROB  x! nPr nCr Ran# P(", "sub:PROB"},
            {"NUMERIC  Abs Int Frac Rnd", "sub:NUM"}, {"ANGLE  \xC2\xB0 \xCA\xB3 \xE1\xB5\x8D \xC2\xB0\xE2\x80\xB2\xE2\x80\xB3 Pol Rec", "sub:ANGLE"},
            {"ESYM  m \xC2\xB5 n p f k M G T P E ENG", "sub:ESYM"}, {"PICTURE  StoPict RclPict", "sub:PICT"},
            {"FUNCMEM  STORE RECALL fn SEE", "sub:FM"}, {"LOGIC  And Or Not Xor", "sub:LOGIC"}, {"CAPTURE  RclCapt", "sub:CAPT"},
            {"TVM  Smpl Cmpd Cash Amt Cnvt", "sub:TVM"}, {"SKETCH  Cls Plot Graph Draw", "sub:SKETCH"},
            {"RELAZIONI  = \xE2\x89\xA0 < > \xE2\x89\xA5 \xE2\x89\xA4", "sub:REL"}, {"RISULTATO  S\xE2\x87\x94" "D ab/c \xE2\x96\xB6" "DMS", "sub:RES"},
            {"BASE-N  d~o LOGIC DISPLAY", "sub:BASEN"}, {"CONST  \xCF\x80 e c h m\xE2\x82\x91 N\xE2\x82\x90 au", "sub:CONST"}};
}
MenuList fnItems(std::initializer_list<std::pair<const char*, const char*>> v) {
    MenuList out;
    for (const auto& e : v) out.push_back({e.first, e.second});
    return out;
}
// Numeri 1..n per FUNCMEM, CAPTURE, Pict.
MenuList numbered(const std::string& label, const std::string& action, int n) {
    MenuList out;
    for (int k = 1; k <= n; ++k) out.push_back({label + std::to_string(k), action + std::to_string(k)});
    return out;
}
// Nome canonico di una variabile di sistema con il suo valore attuale (VARS).
MenuItem systemItem(Context& ctx, const char* token) {
    const char* shown = calclib::displayName(token);
    std::string label = shown ? shown : token;
    double v = 0;
    try { if (ctx.ws.resolve(token, v)) label += " = " + formatReal(v, ctx.ws.settings.display); } catch (const std::exception&) {}
    return {label, std::string("sym:") + token};
}
MenuList optnSub(Context& ctx, const std::string& name) {
    if (name == "LIST") return fnItems({{"List", "sym:List"}, {"List \"nome\"", "txt:List\""}, {"{", "txt:{"}, {"}", "txt:}"},
        {"List\xE2\x86\x92Mat(", "fn:listtomat"}, {"Dim", "fn:Dim"}, {"Fill(", "fn:Fill"}, {"Seq(", "fn:Seq"}, {"Min(", "fn:Min"},
        {"Max(", "fn:Max"}, {"Mean(", "fn:Mean"}, {"Median(", "fn:Median"}, {"Augment(", "fn:Augment"}, {"Sum", "fn:Sum"},
        {"Prod", "fn:Prod"}, {"Cuml", "fn:Cuml"}, {"%", "fn:Percent"}, {"\xCE\x94List", "fn:dlist"}, {"SortA(", "fn:SortA"},
        {"SortD(", "fn:SortD"}, {"List Ans", "txt:ListAns"}});
    if (name == "MAT") return fnItems({{"\xE2\x96\xB6" "Editor MAT/VCT", "editor"}, {"Mat", "sym:Mat"}, {"Mat\xE2\x86\x92List(", "fn:mattolist"},
        {"Det", "fn:Det"}, {"Trn", "fn:Trn"}, {"Augment(", "fn:Augment"}, {"Identity", "fn:Identity"}, {"Dim", "fn:Dim"},
        {"Fill(", "fn:Fill"}, {"Ref", "fn:Ref"}, {"Rref", "fn:Rref"}, {"Vct", "sym:Vct"}, {"DotP(", "fn:DotP"}, {"CrossP(", "fn:CrossP"},
        {"Angle(", "fn:Angle"}, {"UnitV(", "fn:UnitV"}, {"Norm(", "fn:Norm"}, {"[", "txt:["}, {"]", "txt:]"}, {"Mat Ans", "txt:MatAns"},
        {"Vct Ans", "txt:VctAns"}, {"Swap A,r1,r2", "sketch:Swap "}, {"*Row k,A,r", "sketch:*Row "}, {"*Row+ k,A,r1,r2", "sketch:*Row+ "},
        {"Row+ A,r1,r2", "sketch:Row+ "}});
    if (name == "CPLX") return fnItems({{"i", "txt:i"}, {"Abs", "fn:abs"}, {"Arg", "fn:arg"}, {"Conjg", "fn:conj"}, {"ReP", "fn:re"},
        {"ImP", "fn:im"}, {"\xE2\x88\xA0", "sym:\xE2\x88\xA0"}, {"\xE2\x96\xB6r\xE2\x88\xA0\xCE\xB8", "cview:1"}, {"\xE2\x96\xB6" "a+bi", "cview:0"}});
    if (name == "CALC") return fnItems({{"Solve(", "fn:solve"}, {"d/dx", "tpl:derivative"}, {"d\xC2\xB2/dx\xC2\xB2", "tpl:derivative2"},
        {"\xE2\x88\xAB" "dx", "tpl:integral"}, {"SolveN(", "fn:solve_n"}, {"FMin(", "fn:f_min"}, {"FMax(", "fn:f_max"},
        {"\xCE\xA3(", "tpl:sum"}, {"\xCE\xA0(", "tpl:prod"}, {"log\xE2\x82\x90" "b", "tpl:logab"}, {"Int\xC3\xB7", "sym:Int\xC3\xB7"},
        {"Rmdr", "sym:Rmdr"}, {"\xE2\x96\xB6Simp", "sym:\xE2\x96\xB6Simp"}});
    if (name == "STAT") return fnItems({{"\xEE\x80\x80(", "fn:x_hat"}, {"\xC5\xB7(", "fn:y_hat"}, {"DIST  \xE2\x96\xB6", "sub:DIST"},
        {"S-Dev(", "fn:s_dev"}, {"Var(", "fn:stat_var"}});
    if (name == "DIST") return fnItems({{"NormPD(", "fn:norm_pd"}, {"NormCD(", "fn:norm_cd"}, {"InvNormCD(", "fn:invnorm_cd"},
        {"tPD(", "fn:t_pd"}, {"tCD(", "fn:t_cd"}, {"InvTCD(", "fn:invt_cd"}, {"ChiPD(", "fn:chi_pd"}, {"ChiCD(", "fn:chi_cd"},
        {"InvChiCD(", "fn:invchi_cd"}, {"FPD(", "fn:f_pd"}, {"FCD(", "fn:f_cd"}, {"InvFCD(", "fn:invf_cd"}, {"BinomialPD(", "fn:binomial_pd"},
        {"BinomialCD(", "fn:binomial_cd"}, {"InvBinomialCD(", "fn:invbinomial_cd"}, {"PoissonPD(", "fn:poisson_pd"},
        {"PoissonCD(", "fn:poisson_cd"}, {"InvPoissonCD(", "fn:invpoisson_cd"}, {"GeoPD(", "fn:geo_pd"}, {"GeoCD(", "fn:geo_cd"},
        {"InvGeoCD(", "fn:invgeo_cd"}, {"HypergeoPD(", "fn:hypergeo_pd"}, {"HypergeoCD(", "fn:hypergeo_cd"},
        {"InvHypergeoCD(", "fn:invhypergeo_cd"}, {"\"L\"  (coda sinistra)", "txt:\"L\""}, {"\"R\"  (coda destra)", "txt:\"R\""},
        {"\"C\"  (centrale)", "txt:\"C\""}});
    if (name == "CONV") {
        MenuList out;
        const char* const* cats = calclib::unitCategories();
        for (std::size_t k = 0; cats[k]; ++k) out.push_back({cats[k], "sub:CONV" + std::to_string(k)});
        return out;
    }
    if (name.rfind("CONV", 0) == 0) {
        MenuList out;
        for (const calclib::UnitInfo* u : calclib::unitsOf(static_cast<std::size_t>(std::atoi(name.c_str() + 4))))
            out.push_back({std::string("[") + u->name + "]", std::string("unit:") + u->name});
        return out;
    }
    if (name == "HYP") return fnItems({{"sinh", "fn:sinh"}, {"cosh", "fn:cosh"}, {"tanh", "fn:tanh"}, {"sinh\xE2\x81\xBB\xC2\xB9", "fn:asinh"},
        {"cosh\xE2\x81\xBB\xC2\xB9", "fn:acosh"}, {"tanh\xE2\x81\xBB\xC2\xB9", "fn:atanh"}});
    if (name == "PROB") return fnItems({{"x!", "txt:!"}, {"nPr", "fn:nPr"}, {"nCr", "fn:nCr"}, {"Ran#", "sym:ran"}, {"RanInt#(", "fn:randint"},
        {"RanNorm#(", "fn:rannorm"}, {"RanBin#(", "fn:ranbin"}, {"RanList#(", "fn:ran_list"}, {"RanSamp#(", "fn:ran_samp"},
        {"P(", "fn:prob_p"}, {"Q(", "fn:prob_q"}, {"R(", "fn:prob_r"}, {"t(", "fn:prob_t"}});
    if (name == "NUM") return fnItems({{"Abs", "fn:abs"}, {"Int", "fn:int"}, {"Frac", "fn:frac"}, {"Rnd", "fn:num_rnd"}, {"Intg", "fn:intg"},
        {"RndFix(", "fn:rndfix"}, {"GCD(", "fn:gcd"}, {"LCM(", "fn:lcm"}, {"MOD(", "fn:mod"}, {"MOD_Exp(", "fn:modexp"}});
    if (name == "ANGLE") return fnItems({{"\xC2\xB0", "sym:\xC2\xB0"}, {"\xCA\xB3", "sym:\xCA\xB3"}, {"\xE1\xB5\x8D", "sym:\xE1\xB5\x8D"},
        {"\xE2\x80\xB2  (minuti)", "sym:\xE2\x80\xB2"}, {"\xE2\x80\xB3  (secondi)", "sym:\xE2\x80\xB3"}, {"\xE2\x96\xB6" "DMS", "view:7"},
        {"Pol(", "fn:pol"}, {"Rec(", "fn:rec"}});
    if (name == "ESYM") return fnItems({{"m  10^-3", "sym:#m"}, {"\xC2\xB5  10^-6", "sym:#u"}, {"n  10^-9", "sym:#n"}, {"p  10^-12", "sym:#p"},
        {"f  10^-15", "sym:#f"}, {"k  10^3", "sym:#k"}, {"M  10^6", "sym:#M"}, {"G  10^9", "sym:#G"}, {"T  10^12", "sym:#T"},
        {"P  10^15", "sym:#P"}, {"E  10^18", "sym:#E"}, {"ENG", "eng:1"}, {"ENG\xE2\x86\x90", "eng:-1"}});
    if (name == "PICT") return fnItems({{"StoPict", "sketch:StoPict "}, {"RclPict", "sketch:RclPict "}, {"BG-Pict", "sketch:BG-Pict "}});
    if (name == "FM") return fnItems({{"STORE  (input in fn)", "sub:FMSTORE"}, {"RECALL  (testo di fn)", "sub:FMRECALL"},
        {"fn  (funzione nei calcoli)", "sub:FMFN"}, {"SEE  (elenco)", "sub:FMSEE"}});
    if (name == "FMSTORE") return numbered("STORE f", "fmstore:", 20);
    if (name == "FMRECALL") return numbered("RECALL f", "fmrecall:", 20);
    if (name == "FMFN") return numbered("f", "fmfn:", 20);
    if (name == "FMSEE") {
        MenuList out;
        for (int k = 1; k <= 20; ++k) {
            const std::string t = ctx.ws.functionMemory(k);
            out.push_back({"f" + std::to_string(k) + ": " + (t.empty() ? "-" : t), "fmfn:" + std::to_string(k)});
        }
        return out;
    }
    if (name == "LOGIC") return fnItems({{"And", "sym:and"}, {"Or", "sym:or"}, {"Not", "sym:not"}, {"Xor", "sym:xor"}});
    if (name == "CAPT") return fnItems({{"RclCapt", "sketch:RclCapt "}, {"\xE2\x96\xB6 Cattura lo schermo (SHIFT 7)", "sub:CAPTURE"}});
    if (name == "CAPTURE") return numbered("Capture ", "capture:", 20);
    if (name == "TVM") return fnItems({{"Smpl_SI(", "fn:smpl_si"}, {"Smpl_SFV(", "fn:smpl_sfv"}, {"Cmpd_PV(", "fn:cmpd_pv"},
        {"Cmpd_FV(", "fn:cmpd_fv"}, {"Cmpd_PMT(", "fn:cmpd_pmt"}, {"Cmpd_n(", "fn:cmpd_n"}, {"Cmpd_I%(", "fn:cmpd_i"},
        {"Cash_NPV(", "fn:cash_npv"}, {"Cash_IRR(", "fn:cash_irr"}, {"Cash_PBP(", "fn:cash_pbp"}, {"Cash_NFV(", "fn:cash_nfv"},
        {"Amt_BAL(", "fn:amt_bal"}, {"Amt_INT(", "fn:amt_int"}, {"Amt_PRN(", "fn:amt_prn"}, {"Amt_\xCE\xA3INT(", "fn:amt_sint"},
        {"Amt_\xCE\xA3PRN(", "fn:amt_sprn"}, {"Cnvt_EFF(", "fn:cnvt_eff"}, {"Cnvt_APR(", "fn:cnvt_apr"}, {"Cost(", "fn:fin_cost"},
        {"Sell(", "fn:fin_sell"}, {"Margin(", "fn:fin_margin"}, {"Days_Prd(", "fn:days_prd"}});
    if (name == "SKETCH") {
        MenuList out;
        for (const char* const* n = graphcmd::menuNames(); *n; ++n) {
            const std::string item = *n;
            out.push_back({item, item == "PxlTest(" ? "fn:pxl_test" : "sketch:" + item});
        }
        return out;
    }
    if (name == "REL") return fnItems({{"=", "txt:="}, {"\xE2\x89\xA0", "sym:\xE2\x89\xA0"}, {">", "txt:>"}, {"<", "txt:<"},
        {"\xE2\x89\xA5", "sym:\xE2\x89\xA5"}, {"\xE2\x89\xA4", "sym:\xE2\x89\xA4"}, {":  (istruzioni)", "txt::"}, {"\xE2\x97\xA2  (mostra)", "sym:\xE2\x97\xA2"}});
    if (name == "RES") return fnItems({{"\xE2\x96\xB6S\xE2\x87\x94" "D", "view:0"}, {"\xE2\x96\xB6" "ab/c\xE2\x87\x94" "d/c", "view:1"},
        {"\xE2\x96\xB6Periodo", "view:2"}, {"\xE2\x96\xB6" "Dec", "view:3"}, {"\xE2\x96\xB6Hex", "view:4"}, {"\xE2\x96\xB6" "Bin", "view:5"},
        {"\xE2\x96\xB6Oct", "view:6"}, {"\xE2\x96\xB6" "DMS", "view:7"}, {"ENG", "eng:1"}, {"ENG\xE2\x86\x90", "eng:-1"},
        {"\xE2\x96\xB6r\xE2\x88\xA0\xCE\xB8", "cview:1"}, {"\xE2\x96\xB6" "a+bi", "cview:0"}});
    if (name == "BASEN") return fnItems({{"d  (decimale)", "sym:0d"}, {"h  (esadecimale)", "sym:0x"}, {"b  (binario)", "sym:0b"},
        {"o  (ottale)", "sym:0o"}, {"Neg", "fn:neg"}, {"Not", "fn:not"}, {"and", "sym:and"}, {"or", "sym:or"}, {"xor", "sym:xor"},
        {"xnor", "sym:xnor"}, {"\xE2\x96\xB6" "Dec", "view:3"}, {"\xE2\x96\xB6Hex", "view:4"}, {"\xE2\x96\xB6" "Bin", "view:5"}, {"\xE2\x96\xB6Oct", "view:6"}});
    // CONST (v1.14.0): gruppo -> costanti, dalla tabella unica (physical_constants.hpp).
    // Ogni voce mostra glifo e nome e inserisce il nome del parser come un token.
    if (name == "CONST") {
        MenuList out;
        const auto groups = physconst::constGroups();
        for (std::size_t g = 0; g < groups.size(); ++g) {
            std::string label = std::string(groups[g].name) + " ";
            std::size_t shown = 0;
            for (const auto& k : physconst::constantsOf(g)) { if (shown++ == 4) break; label += std::string(" ") + k.glyph; }
            out.push_back({label, "sub:CONST" + std::to_string(g)});
        }
        return out;
    }
    if (name.size() > 5 && name.compare(0, 5, "CONST") == 0) {
        MenuList out;
        for (const auto& k : physconst::constantsOf(static_cast<std::size_t>(std::atoi(name.c_str() + 5))))
            out.push_back({std::string(k.glyph) + "  " + k.nameIT, std::string("sym:") + k.parserName});
        return out;
    }
    return {};
}
// VARS di Run-Matrix (fx-CG50: V-WIN, FACTOR, STAT, GRAPH, DYNA, TABLE,
// RECURSION, EQUATION, TVM, Str) e le memorie A~Z, r, θ e Ans.
MenuList varsRoot() {
    return {{"A~Z, r, \xCE\xB8  variabili", "sub:VAZ"}, {"Ans  PreAns", "sub:VANS"}, {"V-WIN  finestra Graph", "sub:VWIN"},
            {"FACTOR  Xfct Yfct", "sub:VFACT"}, {"STAT  n \xEE\x80\x81 \xCF\x83x a b r", "sub:VSTAT"}, {"GRAPH  Y r Xt Yt X", "sub:VGRAPH"},
            {"DYNA  Start End Pitch", "sub:VDYNA"}, {"TABLE  Start End Pitch Result", "sub:VTABLE"},
            {"RECURSION  an+1 a0 R Start", "sub:VREC"}, {"EQUATION  SimRes PlyRes", "sub:VEQ"}, {"TVM  n I% PV PMT FV", "sub:VTVM"},
            {"Str  Str 1-20", "sub:VSTR"}};
}
MenuList varsSub(Context& ctx, const std::string& name) {
    const CalculatorMemory& m = ctx.ws.memory;
    const DisplayFormat f = ctx.ws.settings.display;
    MenuList out;
    auto add = [&](std::initializer_list<const char*> tokens) { for (const char* t : tokens) out.push_back(systemItem(ctx, t)); };
    if (name == "VAZ") {
        for (char v = 'A'; v <= 'Z'; ++v) {
            const ExactSlot& e = m.exactSlot(v);
            const bool exactShown = e.valid && !e.decimal && !e.value.isZero() && exact::fitsRelaxed(e.value);
            out.push_back({std::string(1, v) + " = " + (exactShown ? exact::toText(e.value, false) : formatReal(m.recall(v), f)), std::string("var:") + v});
        }
        out.push_back({"r = " + formatReal(m.recall(CalculatorMemory::RSlot), f), "sym:r_var"});
        out.push_back({"\xCE\xB8 = " + formatReal(m.recall(CalculatorMemory::ThetaSlot), f), "sym:theta"});
    } else if (name == "VANS") {
        out = {{"Ans = " + formatValue({m.answer(), m.answerImag()}, ctx.ws.settings.complex, f), "txt:Ans"},
               {"PreAns = " + formatValue({m.previousAnswer(), m.previousAnswerImag()}, ctx.ws.settings.complex, f), "txt:PreAns"}};
    } else if (name == "VWIN") add({"xmin", "xmax", "xscl", "ymin", "ymax", "yscl", "tmin", "tmax", "tptch"});
    else if (name == "VFACT") add({"xfct", "yfct"});
    else if (name == "VSTAT") add({"stat_n", "stat_meanx", "stat_sumx", "stat_sumx2", "stat_popsdx", "stat_sdx", "stat_minx", "stat_maxx",
                                   "stat_meany", "stat_sumy", "stat_sumy2", "stat_sumxy", "stat_popsdy", "stat_sdy", "stat_miny", "stat_maxy",
                                   "reg_a", "reg_b", "reg_c", "reg_d", "reg_e", "reg_r", "reg_r2", "reg_mse", "stat_q1", "stat_q3", "stat_med",
                                   "stat_mod", "stat_hstart", "stat_hpitch"});
    else if (name == "VGRAPH") {
        for (const char* kind : {"Y", "r", "Xt", "Yt", "X"})
            for (int k = 1; k <= 20; ++k) out.push_back({std::string(kind) + std::to_string(k), std::string("sym:@") + kind + std::to_string(k)});
    } else if (name == "VDYNA") add({"d_start", "d_end", "d_pitch"});
    else if (name == "VTABLE") { add({"tblstart", "tblend", "tblpitch"}); out.push_back({"Result  (matrice della tabella)", "sym:tbl_result"}); }
    else if (name == "VREC") {
        out.push_back({"an+1  (\"formula\"\xE2\x86\x92" "an+1)", "sym:rec_an1"});
        out.push_back({"an  (nella formula)", "sym:rec_an"});
        out.push_back({"n  (nella formula)", "sym:rec_n"});
        add({"r_start", "r_end", "rec_a0", "rec_a1", "rec_a2", "rec_b0", "rec_b1", "rec_b2", "rec_c0", "rec_c1", "rec_c2",
             "rec_anstart", "rec_bnstart", "rec_cnstart"});
    } else if (name == "VEQ") {
        // VARS › EQUATION (manuale 1-33): matrici di soluzioni e coefficienti, lette
        // dal workspace con CollectionResolver (il risultato va in MatAns).
        for (int k = 0; k < 4; ++k) {
            CalcValue v;
            const bool defined = ctx.ws.collection(CollectionResolver::Memory::Equation, k, v) && !v.empty();
            out.push_back({std::string(equationMatrixName(k)) + (defined ? "  " + dimensionText(v) : "  (vuota)"),
                           std::string("sym:") + equationMatrixName(k)});
        }
    }
    else if (name == "VTVM") add({"tvm_n", "tvm_i", "tvm_pv", "tvm_pmt", "tvm_fv", "tvm_py", "tvm_cy"});
    else if (name == "VSTR") {
        for (int k = 1; k <= 20; ++k) {
            const std::string& t = ctx.ws.strings[static_cast<std::size_t>(k - 1)];
            out.push_back({"Str " + std::to_string(k) + (t.empty() ? std::string() : "  \"" + t + "\""), "sym:str_" + std::to_string(k)});
        }
    }
    return out;
}
// CATALOG (SHIFT 4): tutti i comandi in ordine alfabetico.
MenuList catalogItems() {
    MenuList out;
    for (const calclib::CatalogEntry* e = calclib::catalog(); e->shown; ++e) {
        const std::string ins = e->insert;
        std::string action;
        if (!ins.empty() && ins.back() == '(') action = "fn:" + ins.substr(0, ins.size() - 1);
        else if (graphcmd::isCommand(ins) || ins.find(' ') != std::string::npos) action = "sketch:" + ins;
        else action = "sym:" + ins;
        out.push_back({e->shown, action});
    }
    std::sort(out.begin(), out.end(), [](const MenuItem& a, const MenuItem& b) {
        std::string x = a.label, y = b.label;
        for (char& ch : x) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        for (char& ch : y) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return x < y;
    });
    return out;
}
// Menu MATH (ordine della fx-CG50: prima MAT/VCT, poi gli altri modelli).
const std::vector<std::string>& mathTemplates() {
    static const std::vector<std::string> t = {
        "MAT/VCT  \xE2\x96\xB6",
        "\xE2\x96\xA1 \xE2\x96\xA1/\xE2\x96\xA1  frazione mista", "\xE2\x81\xBF\xE2\x88\x9A\xE2\x96\xA1  radice n-esima", "\xE2\x88\x9B\xE2\x96\xA1  radice cubica",
        "\xE2\x96\xA1\xC2\xB2  quadrato", "\xE2\x96\xA1\xE2\x81\xBB\xC2\xB9  reciproco", "log\xE2\x96\xA1(\xE2\x96\xA1)  log base a", "e^\xE2\x96\xA1", "10^\xE2\x96\xA1",
        "|\xE2\x96\xA1|  valore assoluto", "d/dx", "\xE2\x88\xAB dx", "\xCE\xA3", "\xCE\xA0", "( )  gruppo", "0.1\xE2\x80\xBE  periodo"};
    return t;
}
// MATH > MAT/VCT (manuale 1-16): modelli di matrici e vettori, m×n fino a 6×6.
const std::vector<std::string>& matrixTemplates() {
    static const std::vector<std::string> t = {
        "2\xC3\x97" "2  matrice", "3\xC3\x97" "3  matrice", "m\xC3\x97n  (fino a 6\xC3\x97" "6)", "2\xC3\x97" "1  vettore",
        "3\xC3\x97" "1  vettore", "1\xC3\x97" "2  vettore", "1\xC3\x97" "3  vettore"};
    return t;
}
// Colori della riga di input: parentesi per livello, evidenziazioni.
math::Palette inputPalette(std::uint16_t ink) {
    math::Palette p;
    p.ink = ink; p.cursor = Cursor; p.placeholder = Placeholder;
    for (int i = 0; i < 5; ++i) p.paren[i] = Paren[i];
    p.selection = Highlight; p.error = ErrorHighlight;
    return p;
}
// Una riga di testo disegnata cosi' com'e' (output Line).
ast::NodePtr textRow(const std::string& text) {
    auto r = ast::make(ast::Kind::Row);
    r->children.push_back(ast::make(ast::Kind::Token, text));
    return r;
}
const char* const kNoExact = "Nessuna altra forma: valore approssimato o intero";
} // namespace

void RunMatrixApp::enter(Context& ctx) {
    ctx.ws.apply(); browse_ = -1; menu_.close(); menuKind_ = Menu::None;
    matrices_.close();
    answers_.close();
    resultScroll_ = 0;
    syncInputMode(ctx);
}
void RunMatrixApp::leave(Context&) { if (editIndex_ >= 0) endEdit(true); editor_.cancelSelection(); editor_.cancelArgument(); }
void RunMatrixApp::syncInputMode(Context& ctx) {
    const bool line = ctx.ws.settings.inputMode == Settings::InputMode::Line;
    if (editor_.linear() != line) editor_.setLinear(line);
}
void RunMatrixApp::markError(const ErrorLocation& where) {
    if (where.known()) (void)editor_.markError(static_cast<std::size_t>(where.position), static_cast<std::size_t>(where.length));
}
// CLIP (fx-CG50 SHIFT 8): le frecce estendono la selezione, F1 COPY, F2 CUT,
// DEL la elimina, EXIT annulla. Gli altri tasti terminano la selezione e
// funzionano come sempre.
bool RunMatrixApp::clipKey(Context& ctx, const KeyEvent& ev) {
    if (ev.is(Key::Char) && ev.ctrl && (ev.ch == 'c' || ev.ch == 'C' || ev.ch == 'x' || ev.ch == 'X')) {
        const bool cut = ev.ch == 'x' || ev.ch == 'X';
        auto copy = cut ? editor_.cutSelection() : editor_.copySelection();
        editor_.cancelSelection();
        if (copy) { ctx.clipboard = copy; ctx.toast(cut ? "Tagliato" : "Copiato"); }
        return true;
    }
    switch (ev.key) {
    case Key::Left: (void)editor_.selectLeft(); return true;
    case Key::Right: (void)editor_.selectRight(); return true;
    case Key::Up: case Key::Down: return true;
    case Key::F1: case Key::F2: {
        const bool cut = ev.is(Key::F2);
        auto copy = cut ? editor_.cutSelection() : editor_.copySelection();
        editor_.cancelSelection();
        if (copy) { ctx.clipboard = copy; ctx.toast(cut ? "Tagliato" : "Copiato"); }
        else ctx.toast("Selezione vuota");
        return true;
    }
    case Key::Del: case Key::DelForward: (void)editor_.deleteSelection(); return true;
    case Key::Exit: case Key::AC: editor_.cancelSelection(); return true;
    default: editor_.cancelSelection(); return false;
    }
}

HistoryEntry* RunMatrixApp::target(Context& ctx) {
    auto& h = ctx.ws.history;
    if (h.empty()) return nullptr;
    if (browse_ >= 0 && browse_ < static_cast<int>(h.size())) return &h[static_cast<std::size_t>(browse_)];
    return &h.back();
}

void RunMatrixApp::resultKey(Context& ctx, int kind) {
    HistoryEntry* e = target(ctx);
    if (!e) return;
    const Settings& s = ctx.ws.settings;
    bool changed = false;
    switch (kind) {
    case 0: changed = toggleExactDecimal(*e, s); if (!changed) ctx.toast(kNoExact); break;
    case 1: changed = toggleMixedFraction(*e, s); if (!changed) ctx.toast("Nessuna frazione impropria da convertire"); break;
    case 2: changed = togglePeriodic(*e); if (!changed) ctx.toast("Il periodo richiede un risultato razionale esatto"); break;
    default: {
        static const std::uint8_t views[] = {ViewDecimal, ViewHex, ViewBin, ViewOct, ViewDms};
        e->view = views[std::clamp(kind - 3, 0, 4)];
        changed = true;
        break;
    }
    }
    if (changed) ctx.markDirty();
}

void RunMatrixApp::showMenu(const std::string& title, const std::vector<std::pair<std::string, std::string>>& items) {
    std::vector<std::string> labels;
    actions_.clear();
    for (const auto& it : items) { labels.push_back(it.first); actions_.push_back(it.second); }
    menuTitle_ = title;                          // PopupMenu conserva solo il puntatore
    menu_.show(menuTitle_.c_str(), std::move(labels));
    menuKind_ = Menu::Action;
}
namespace {
std::vector<std::pair<std::string, std::string>> pairs(const MenuList& l) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& it : l) out.emplace_back(it.label, it.action);
    return out;
}
} // namespace
void RunMatrixApp::openOptn(Context&) { showMenu("OPTN", pairs(optnRoot())); }
void RunMatrixApp::openVars(Context&) { showMenu("VARS", pairs(varsRoot())); }
void RunMatrixApp::openCatalog(Context&) { showMenu("CATALOG", pairs(catalogItems())); }

// Esegue l'azione di una voce di menu (vedi MenuItem).
void RunMatrixApp::runAction(Context& ctx, const std::string& a, const std::string& label) {
    const std::size_t colon = a.find(':');
    const std::string verb = a.substr(0, colon), arg = colon == std::string::npos ? std::string() : a.substr(colon + 1);
    if (verb == "sub") {
        MenuList items = arg.rfind("V", 0) == 0 ? varsSub(ctx, arg) : optnSub(ctx, arg);
        std::string title = label.substr(0, label.find("  "));
        showMenu(title, pairs(items));
        return;
    }
    if (verb == "editor") { matrices_.open(ctx); return; }
    if (verb == "sym") { editor_.insertSymbol(arg); return; }
    if (verb == "fn") { editor_.insertSymbol(arg); editor_.insert('('); return; }
    if (verb == "txt") {
        if (arg == "ListAns" || arg == "MatAns" || arg == "VctAns") { editor_.insertSymbol(arg.substr(0, arg.size() - 3)); editor_.insertText("Ans"); return; }
        if (arg == "List\"") { editor_.insertSymbol("List"); editor_.insert('"'); return; }
        editor_.insertText(arg);
        return;
    }
    if (verb == "tpl") {
        if (arg == "integral") editor_.insertIntegral();
        else if (arg == "derivative") editor_.insertDerivative();
        else if (arg == "derivative2") editor_.insertSecondDerivative();
        else if (arg == "sum") editor_.insertSummation();
        else if (arg == "prod") editor_.insertProduct();
        else if (arg == "logab") editor_.insertLogBase();
        return;
    }
    if (verb == "view") { resultKey(ctx, std::atoi(arg.c_str())); return; }
    if (verb == "eng" || verb == "cview") {
        HistoryEntry* e = target(ctx);
        const bool ok = e && (verb == "eng" ? shiftEngineering(*e, std::atoi(arg.c_str())) : setComplexView(*e, arg == "1"));
        if (ok) ctx.markDirty(); else ctx.toast(verb == "eng" ? "ENG: serve un risultato reale" : "Serve un risultato complesso");
        return;
    }
    if (verb == "unit") {
        // Prima unita' dopo il valore: [cm]; la seconda e' la destinazione: ▶[in].
        const std::string text = editor_.toExpression();
        const std::size_t open = text.rfind('['), close = text.rfind(']');
        const bool afterUnit = open != std::string::npos && close == text.size() - 1 && calclib::findUnit(text.substr(open + 1, close - open - 1)) &&
                               (open < 3 || text.compare(open - 3, 3, "\xE2\x96\xB6") != 0);
        editor_.insertSymbol((afterUnit ? "\xE2\x96\xB6[" : "[") + arg + "]");
        return;
    }
    if (verb == "var") {
        // In Hex le lettere A-F sono cifre: la variabile si scrive v_a (mostrata sottolineata).
        const char v = arg.empty() ? 'A' : arg[0];
        if (ctx.ws.settings.baseMode == Settings::BaseMode::Hex && v >= 'A' && v <= 'F') editor_.insertSymbol(std::string("v_") + static_cast<char>(std::tolower(static_cast<unsigned char>(v))));
        else editor_.insert(v);
        return;
    }
    if (verb == "sketch") {
        if (editor_.linear()) editor_.insertSymbol(arg);
        else {
            // In Math il nome resta un solo token (la riga si legge come comando).
            const std::string trimmed = !arg.empty() && arg.back() == ' ' ? arg.substr(0, arg.size() - 1) : arg;
            editor_.insertSymbol(trimmed + (!arg.empty() && arg.back() == ' ' ? " " : ""));
        }
        return;
    }
    const int n = std::atoi(arg.c_str());
    if (verb == "fmstore") {
        ctx.ws.setFunctionMemory(static_cast<std::size_t>(n - 1), editor_.toExpression());
        ctx.markDirty();
        ctx.toast("Memorizzato in f" + std::to_string(n));
        return;
    }
    if (verb == "fmrecall") {
        const std::string t = ctx.ws.functionMemory(n);
        if (t.empty()) { ctx.toast("f" + std::to_string(n) + " vuota"); return; }
        NaturalEditor tmp;
        tmp.setLinear(editor_.linear());
        tmp.setExpression(t);
        editor_.paste(tmp.root());
        return;
    }
    if (verb == "fmfn") { editor_.insertSymbol("f_" + std::to_string(n)); return; }
    if (verb == "capture") { writeCapture(ctx, n); return; }
}

// CAPTURE (SHIFT 7, v1.12.0): la schermata di Run-Matrix in Capture n
// (PICOCALC/CAPT/CAPTnn.pcpict sull'archivio scelto in SYSTEM); RclCapt n la mostra.
void RunMatrixApp::takeCapture(Context& ctx) {
    capture_ = pict::Image{};
    const gfx::Canvas* screen = ctx.screen();
    if (!screen || !pict::encode(screen->pixels(), screen->width(), screen->width(), screen->height(), capture_)) {
        ctx.toast("Memory ERROR: schermata non catturabile");
        return;
    }
    showMenu("CAPTURE", pairs(numbered("Capture ", "capture:", 20)));
}
void RunMatrixApp::writeCapture(Context& ctx, int n) {
    if (capture_.empty()) { ctx.toast("Nessuna schermata catturata (SHIFT 7)"); return; }
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) throw MathError("Storage ERROR: archivio non disponibile");
    char name[32];
    std::snprintf(name, sizeof(name), "CAPT%02d%s", n, files::PictExtension);
    const files::Status dir = fs->makeDirs(graphcmd::CaptureFolder);
    if (dir != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(dir));
    const auto data = pict::serialize(capture_);
    const files::Status st = files::writeAtomic(*fs, files::join(graphcmd::CaptureFolder, name), data.data(), data.size());
    capture_ = pict::Image{};
    if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
    ctx.toast("Capture " + std::to_string(n) + " salvata");
}

void RunMatrixApp::evaluate(Context& ctx) {
    // In caso di errore il cursore va sulla causa nell'input (fx-CG50: "Cursor
    // is positioned automatically at the location of the cause of the error") e
    // la causa viene evidenziata; la finestra d'errore la mostra l'AppManager.
    try {
    if (editIndex_ >= 0) {
        // fx-CG50 history editing: the edited line and every line after it are recalculated.
        const std::size_t failed = editHistoryEntry(ctx.ws, static_cast<std::size_t>(editIndex_), editor_.toExpression());
        const int edited = editIndex_;
        endEdit(true);
        browse_ = -1;
        ctx.markDirty();
        const std::size_t later = ctx.ws.history.size() - static_cast<std::size_t>(edited) - 1;
        if (failed) ctx.toast(std::to_string(failed) + " righe successive con errore");
        else if (later) ctx.toast("Ricalcolate " + std::to_string(later + 1) + " righe");
        // Una riga di comando grafico modificata si riesegue: il disegno si vede in GRAPH.
        if (ctx.ws.history[static_cast<std::size_t>(edited)].command) ctx.open(AppId::Graph);
        return;
    }
    if (editor_.empty()) {
        // EXE on an empty line repeats the previous calculation, like the CG50.
        if (ctx.ws.history.empty()) return;
        editor_.setExpression(ctx.ws.history.back().expression);
    }
    const std::string text = editor_.toExpression();
    const RunOutcome out = runCalculation(ctx.ws, text);
    editor_.clear();
    scrollX_ = 0;
    resultScroll_ = 0;
    ctx.markDirty();
    if (!out.storedIn.empty()) ctx.toast("Memorizzato in " + out.storedIn);
    // Comando grafico (Plot, F-Line, Graph Y=...): GRAPH mostra il disegno, EXIT torna qui.
    if (out.graphics) { ctx.open(AppId::Graph); return; }
    // Risultato oltre la vista naturale (> 6x6, > 20 elementi): schermata MatAns / ListAns.
    const HistoryEntry& last = ctx.ws.history.back();
    if (last.collection && last.collection->value.empty()) answers_.open(ctx, last.collection->value.type);
    }
    catch (const SyntaxError& e) { markError(e.where); throw; }
    catch (const MathError& e) { markError(e.where); throw; }
}

void RunMatrixApp::beginEdit(Context& ctx, int index) {
    if (index < 0 || index >= static_cast<int>(ctx.ws.history.size())) return;
    pendingInput_ = editor_.copyTree();
    editor_.setExpression(ctx.ws.history[static_cast<std::size_t>(index)].expression);
    editIndex_ = index;
    browse_ = -1;
    scrollX_ = 0;
}
void RunMatrixApp::endEdit(bool restoreInput) {
    editIndex_ = -1;
    if (restoreInput && pendingInput_) editor_.setTree(*pendingInput_);
    else editor_.clear();
    pendingInput_.reset();
    scrollX_ = 0;
}

void RunMatrixApp::menuChoice(Context& ctx, int choice) {
    const auto idx = static_cast<std::size_t>(choice);
    switch (menuKind_) {
    case Menu::Math:
        switch (choice - 1) {
        case -1:
            menu_.show("MAT/VCT", matrixTemplates());
            menuKind_ = Menu::MatVct;
            return;
        case 0: editor_.insertMixedFraction(); break;
        case 1: editor_.insertRootTemplate(); break;      // index and radicand are both editable
        case 2: editor_.insertNthRoot(3); break;
        case 3: editor_.insertSquare(); break;
        case 4: editor_.insertPower(); editor_.insertText("-1"); (void)editor_.moveRight(); break;
        case 5: editor_.insertLogBase(); break;
        case 6: editor_.insertExp(); break;
        case 7: editor_.insertTenPower(); break;
        case 8: editor_.insertAbs(); break;
        case 9: editor_.insertDerivative(); break;
        case 10: editor_.insertIntegral(); break;
        case 11: editor_.insertSummation(); break;
        case 12: editor_.insertProduct(); break;
        case 13: editor_.insertGroup(); break;
        case 14: editor_.insertPeriod(); break;
        default: break;
        }
        menuKind_ = Menu::None;
        break;
    case Menu::MatVct: {
        if (choice == 2) {                                   // m×n: righe, poi colonne
            menu_.show("MAT/VCT: righe m", {"1", "2", "3", "4", "5", "6"});
            menuKind_ = Menu::MatRows;
            return;
        }
        static const std::size_t sizes[][2] = {{2, 2}, {3, 3}, {0, 0}, {2, 1}, {3, 1}, {1, 2}, {1, 3}};
        menuKind_ = Menu::None;
        if (idx < 7) editor_.insertMatrix(sizes[idx][0], sizes[idx][1]);
        break;
    }
    case Menu::MatRows:
        matRows_ = idx + 1;
        menu_.show("MAT/VCT: colonne n", {"1", "2", "3", "4", "5", "6"});
        menuKind_ = Menu::MatCols;
        break;
    case Menu::MatCols:
        menuKind_ = Menu::None;
        editor_.insertMatrix(matRows_, idx + 1);
        break;
    case Menu::Action: {
        const std::string action = idx < actions_.size() ? actions_[idx] : std::string();
        const std::string label = idx < menu_.items.size() ? menu_.items[idx] : std::string();
        menuKind_ = Menu::None;
        runAction(ctx, action, label);
        break;
    }
    case Menu::None: break;
    }
}

bool RunMatrixApp::browseKey(Context& ctx, const KeyEvent& ev) {
    auto& hist = ctx.ws.history;
    const int n = static_cast<int>(hist.size());
    if (browse_ >= n) browse_ = n - 1;
    if (browse_ < 0) return false;
    const bool shift = ctx.shift();
    if (ev.is(Key::Char) && ev.ctrl && (ev.ch == 'c' || ev.ch == 'C')) {
        NaturalEditor tmp;
        tmp.setExpression(hist[static_cast<std::size_t>(browse_)].expression);
        ctx.clipboard = tmp.copyTree();
        ctx.toast("Espressione copiata");
        return true;
    }
    switch (ev.key) {
    case Key::Up: if (browse_ > 0) --browse_; resultScroll_ = 0; return true;
    case Key::Down: if (browse_ + 1 < n) ++browse_; else browse_ = -1; resultScroll_ = 0; return true;
    case Key::Exe: case Key::F1: beginEdit(ctx, browse_); return true;          // edit in place and recalculate
    case Key::Left: case Key::Right:
        // Risultato piu' largo della riga: le frecce lo scorrono (v1.11.0).
        if (resultOverflow_ > 0) {
            resultScroll_ = std::clamp(resultScroll_ + (ev.is(Key::Right) ? 48 : -48), 0, resultOverflow_);
            return true;
        }
        [[fallthrough]];
    case Key::F2:                                                              // reuse the expression in the input line
        editor_.setExpression(hist[static_cast<std::size_t>(browse_)].expression);
        browse_ = -1; return true;
    case Key::F3: {                                                            // reuse the result (exact when possible)
        const std::string v = resultExpression(hist[static_cast<std::size_t>(browse_)]);
        browse_ = -1;
        NaturalEditor tmp;
        tmp.setExpression(v);
        editor_.paste(tmp.root());
        return true;
    }
    case Key::F4: {
        // Un risultato lista / matrice oltre la vista naturale si legge in MatAns / ListAns.
        const HistoryEntry& h = hist[static_cast<std::size_t>(browse_)];
        if (h.collection) { answers_.open(ctx, h.collection->value.type); return true; }
        resultKey(ctx, 2); return true;
    }
    case Key::F5: resultKey(ctx, shift ? 1 : 0); return true;
    case Key::F6: case Key::Del: case Key::DelForward:
        hist.erase(hist.begin() + browse_); ctx.markDirty();
        browse_ = std::min(browse_, static_cast<int>(hist.size()) - 1);
        return true;
    case Key::Exit: case Key::AC: browse_ = -1; return true;
    default: browse_ = -1; return false;   // other keys go back to the input line and are handled there
    }
}

void RunMatrixApp::key(Context& ctx, const KeyEvent& ev) {
    syncInputMode(ctx);
    // --- editor MAT/VCT (SHIFT 3) e schermate MatAns / ListAns
    if (matrices_.isOpen()) { matrices_.key(ctx, ev); return; }
    if (answers_.isOpen()) { answers_.key(ctx, ev); return; }
    // L'evidenziazione della causa di un errore dura fino al tasto successivo.
    editor_.clearErrorMark();
    // --- popup menus
    if (menu_.open) {
        const int choice = menu_.key(ev);
        if (choice == -2) menuKind_ = Menu::None;
        if (choice < 0) return;
        menuChoice(ctx, choice);
        return;
    }
    // --- history browsing (UP from the input line)
    if (browse_ >= 0 && browseKey(ctx, ev)) return;
    // --- selezione CLIP
    if (editor_.selecting() && clipKey(ctx, ev)) return;
    const bool shift = ctx.shift();
    // fx-CG50 SHIFT 3 'MAT/VCT', SHIFT 8 CLIP, SHIFT 9 PASTE.
    if (shift && ev.is(Key::Char) && !ev.ctrl) {
        if (ev.ch == '3') { matrices_.open(ctx); return; }
        if (ev.ch == '4') { openCatalog(ctx); return; }                  // CATALOG (v1.12.0)
        if (ev.ch == '7') { takeCapture(ctx); return; }                  // CAPTURE (v1.12.0)
        if (ev.ch == '8') { editor_.beginSelection(); return; }
        if (ev.ch == '9') { if (ctx.clipboard) editor_.paste(*ctx.clipboard); else ctx.toast("Appunti vuoti"); return; }
    }
    if (ev.is(Key::Char) && ev.ctrl) {
        switch (ev.ch) {
        case 'c': case 'C': ctx.clipboard = editor_.copyTree(); ctx.toast("Copiato"); return;
        case 'x': case 'X': ctx.clipboard = editor_.copyTree(); editor_.clear(); ctx.toast("Tagliato"); return;
        case 'v': case 'V': if (ctx.clipboard) editor_.paste(*ctx.clipboard); return;
        case 'z': case 'Z': if (!editor_.undo()) ctx.toast("Niente da annullare"); return;
        case 'y': case 'Y': if (!editor_.redo()) ctx.toast("Niente da ripetere"); return;
        default: return;
        }
    }
    switch (ev.key) {
    case Key::Char:
        // Alt+lettera (v1.12.0): la variabile A-Z scritta esplicitamente (v_a), distinta
        // dalle cifre esadecimali A-F come le lettere di ALPHA della fx-CG50.
        if (ev.alt && std::isalpha(static_cast<unsigned char>(ev.ch))) {
            editor_.insertSymbol(std::string("v_") + static_cast<char>(std::tolower(static_cast<unsigned char>(ev.ch))));
            return;
        }
        try { editor_.insert(ev.ch); }
        catch (const std::invalid_argument&) { ctx.toast("Carattere non ammesso"); }
        return;
    case Key::Exe: evaluate(ctx); return;
    case Key::Exit:
        if (editIndex_ >= 0) { const int i = editIndex_; endEdit(true); browse_ = i; return; }   // cancel the edit
        if (!editor_.empty()) editor_.clear(); else ctx.home();
        return;
    case Key::AC:
        if (editIndex_ >= 0) { endEdit(true); return; }
        if (!editor_.empty()) editor_.clear();
        else { ctx.ws.history.clear(); ctx.markDirty(); }
        return;
    case Key::Del:
        // INS (fx-CG50 SHIFT DEL): sovrascrittura in Line, argomento di un modello in Math.
        if (shift) {
            if (editor_.linear()) { editor_.setOverwrite(!editor_.overwrite()); ctx.toast(editor_.overwrite() ? "Sovrascrittura" : "Inserimento"); }
            else { editor_.beginArgument(); ctx.toast("INS: il prossimo modello prende il valore a destra"); }
            return;
        }
        editor_.backspace(); return;
    case Key::DelForward: editor_.eraseForward(); return;
    case Key::Left: editor_.moveLeft(); return;
    case Key::Right: editor_.moveRight(); return;
    case Key::Up:
        if (editor_.cursorPath().empty() || !editor_.moveUp()) {
            if (editIndex_ < 0 && editor_.cursorPath().empty() && !ctx.ws.history.empty()) browse_ = static_cast<int>(ctx.ws.history.size()) - 1;
        }
        return;
    case Key::Down: editor_.moveDown(); return;
    case Key::Home: editor_.moveHome(); return;
    case Key::End: editor_.moveEnd(); return;
    case Key::Store: editor_.insertSymbol("\xE2\x86\x92"); return;
    case Key::F1:
        if (!shift) { editor_.insertFraction(); return; }
        try { editor_.insertMixedFraction(); } catch (const std::invalid_argument& e) { ctx.toast(e.what()); }
        return;
    case Key::F2: if (shift) editor_.insertSquare(); else editor_.insertRadical(); return;
    case Key::F3: if (shift) editor_.insertText("pi"); else editor_.insertPower(); return;
    case Key::F4: if (shift) editor_.insertSymbol("\xE2\x86\x92"); else { menu_.show("MATH", mathTemplates()); menuKind_ = Menu::Math; } return;
    case Key::F5: resultKey(ctx, shift ? 1 : 0); return;          // S<->D, SHIFT: ab/c <-> d/c
    case Key::F6: if (shift) editor_.insertText("theta"); else editor_.insertText("Ans"); return;
    case Key::Optn: openOptn(ctx); return;
    case Key::Vars: openVars(ctx); return;
    default: return;
    }
}

void RunMatrixApp::draw(Context& ctx, gfx::Canvas& c) {
    syncInputMode(ctx);
    if (matrices_.isOpen()) { matrices_.draw(ctx, c); return; }
    if (answers_.isOpen()) { answers_.draw(ctx, c); return; }
    ui::clear(c);
    // Titolo: CLIP, modifica, Base-N (Dec/Hex/Bin/Oct) e INS come sulla barra della fx-CG50.
    static const char* const kModes[] = {"", "  Dec", "  Hex", "  Bin", "  Oct"};
    std::string title = editor_.selecting() ? "RUN-MATRIX  CLIP" : editIndex_ >= 0 ? "RUN-MATRIX  modifica" : "RUN-MATRIX";
    title += kModes[std::min<std::size_t>(static_cast<std::size_t>(ctx.ws.settings.baseMode), 4)];
    if (editor_.overwrite()) title += "  OVR";
    ctx.drawStatus(c, title.c_str());
    const int top = ui::ContentTop + 2, bottom = ui::ContentBottom - 2, left = 4, width = ui::ScreenW - 8;
    const Settings& s = ctx.ws.settings;
    const bool lineMode = s.inputMode == Settings::InputMode::Line;
    const auto& hist = ctx.ws.history;
    math::CursorTarget cursor{editor_.activeRowPtr(), editor_.cursorIndex()};
    // Parentesi colorate per livello durante la digitazione; evidenziata la
    // selezione (CLIP) o la causa dell'ultimo errore.
    cursor.colorParens = browse_ < 0;
    cursor.cursorStyle = editor_.overwrite() ? 1 : editor_.argumentPending() ? 2 : 0;
    const NaturalEditor::Range mark = editor_.selecting() ? editor_.selection() : editor_.errorMark();
    if (!mark.empty()) { cursor.markRow = mark.row; cursor.markBegin = mark.begin; cursor.markEnd = mark.end; cursor.markIsError = !editor_.selecting(); }
    const math::Size edSize = math::measure(editor_.root());
    const int editorH = std::max(18, edSize.height + 6);

    // Blocks from the top: each history entry is expression (left) + result (right),
    // then the input line (absent while an entry is edited in place).
    struct Block { int h; int entry; bool result; bool live; };
    std::vector<Block> blocks;
    std::vector<ast::NodePtr> exprTrees(hist.size()), resultTrees(hist.size());
    for (std::size_t i = 0; i < hist.size(); ++i) {
        const bool live = editIndex_ == static_cast<int>(i);
        if (!live) {
            // Math: l'espressione disegnata in 2D; Line: su una riga, come digitata.
            NaturalEditor tmp;
            tmp.setLinear(lineMode);
            try { tmp.setExpression(lineMode ? NaturalEditor::toLinearText(hist[i].expression) : hist[i].expression); } catch (...) { tmp.clear(); tmp.insertText("?"); }
            exprTrees[i] = tmp.copyTree();
        }
        resultTrees[i] = lineMode ? textRow(resultText(hist[i], s, true)) : resultTree(hist[i], s);
        // Risultati intermedi di ◢ (v1.12.0): prima del risultato finale, come "- Disp -" della fx-CG50.
        if (!hist[i].displays.empty() && hist[i].error.empty()) {
            auto combined = ast::make(ast::Kind::Row);
            for (const std::string& d : hist[i].displays) {
                combined->children.push_back(ast::make(ast::Kind::Token, d));
                combined->children.push_back(ast::make(ast::Kind::Token, "  \xE2\x97\xA2  "));
            }
            for (const auto& ch : resultTrees[i]->children) combined->children.push_back(ch);
            resultTrees[i] = combined;
        }
        blocks.push_back({live ? editorH : math::measure(*exprTrees[i]).height + 4, static_cast<int>(i), false, live});
        blocks.push_back({math::measure(*resultTrees[i]).height + 6, static_cast<int>(i), true, false});
    }
    if (editIndex_ < 0) blocks.push_back({editorH, -1, false, true});
    // Scroll so that the active block (input, edited or browsed entry) is visible.
    int total = 0, focusY = 0, focusH = 0;
    const int focusEntry = editIndex_ >= 0 ? editIndex_ : browse_;
    for (const auto& b : blocks) {
        if ((focusEntry < 0 && b.entry == -1) || (focusEntry >= 0 && b.entry == focusEntry)) {
            if (focusH == 0) focusY = total;
            focusH = total + b.h - focusY;
        }
        total += b.h;
    }
    int offset = std::min(0, (bottom - top) - total);
    if (top + offset + focusY < top) offset = -focusY;
    if (top + offset + focusY + focusH > bottom) offset = (bottom - top) - (focusY + focusH);
    int y = top + offset;
    resultOverflow_ = 0;
    for (const auto& b : blocks) {
        const bool sel = b.entry >= 0 && b.entry == browse_;
        if (b.live) {
            // Input line or entry edited in place, with cursor and horizontal scrolling.
            const int baseY = y + 3 + edSize.baseline;
            auto l = math::layout(editor_.root(), left - scrollX_, baseY, width + scrollX_ + 2000, editorH, math::Align::Left, &cursor);
            if (l.hasCursor) {
                if (l.cursorX - left > width - 4) scrollX_ += (l.cursorX - left) - (width - 4);
                if (l.cursorX < left) scrollX_ = std::max(0, scrollX_ - (left - l.cursorX) - 8);
                l = math::layout(editor_.root(), left - scrollX_, baseY, width + scrollX_ + 2000, editorH, math::Align::Left, &cursor);
            }
            if (y + b.h >= top && y < bottom) {
                c.setClip({0, top, ui::ScreenW, bottom - top});
                if (b.entry >= 0) c.rect(0, y, ui::ScreenW, b.h, SelectSoft, true);
                c.setClip({left, std::max(top, y), width, std::min(bottom, y + b.h) - std::max(top, y)});
                if (browse_ < 0) math::draw(c, l, inputPalette(Ink));
                else math::draw(c, l, Muted, Bg, Placeholder);
                c.resetClip();
                if (scrollX_ > 0) c.rect(0, y + 2, 2, b.h - 4, Accent, true);
            }
        } else if (y + b.h >= top && y < bottom) {
            c.setClip({0, top, ui::ScreenW, bottom - top});
            const auto& tree = b.result ? *resultTrees[static_cast<std::size_t>(b.entry)] : *exprTrees[static_cast<std::size_t>(b.entry)];
            const math::Size sz = math::measure(tree);
            if (sel && !b.result) c.rect(0, y, ui::ScreenW, b.h, Select, true);
            const HistoryEntry& h = hist[static_cast<std::size_t>(b.entry)];
            const bool failed = b.result && !h.error.empty() && h.error[0] != '=';
            // Risultato piu' largo della riga (v1.11.0): allineato a sinistra e scorrevole
            // con ←/→ quando la riga e' selezionata; ◀ ▶ indicano le parti nascoste.
            const bool wide = b.result && sz.width > width;
            int shift = 0;
            if (wide && sel) { resultOverflow_ = sz.width - width; resultScroll_ = std::min(resultScroll_, resultOverflow_); shift = resultScroll_; }
            auto l = wide ? math::layout(tree, left - shift, y + 2 + sz.baseline, width + shift + sz.width, b.h, math::Align::Left)
                          : math::layout(tree, left, y + 2 + sz.baseline, width, b.h, b.result ? math::Align::Right : math::Align::Left);
            c.setClip({left, std::max(top, y), width, std::min(bottom, y + b.h) - std::max(top, y)});
            math::draw(c, l, sel && !b.result ? SelectInk : failed ? Error : Ink, Cursor, Placeholder);
            c.setClip({0, top, ui::ScreenW, bottom - top});
            if (wide) {
                if (shift > 0) ui::text(c, 0, y + 2, "\xE2\x97\x80", Accent, font::Face::Small);
                if (shift < sz.width - width) ui::text(c, ui::ScreenW - 6, y + 2, "\xE2\x96\xB6", Accent, font::Face::Small);
            }
            if (b.result) c.hLine(left, y + b.h - 2, width, Grid);
            c.resetClip();
        }
        y += b.h;
    }
    // History length indicator (fx-CG50 scroll bar).
    if (total > bottom - top) ui::scrollBar(c, ui::ScreenW - 3, top, bottom - top, total, bottom - top, -offset);

    const bool shift = ctx.shift();
    if (editor_.selecting()) {
        const char* keys[6] = {"COPY", "CUT", nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, keys);
    } else if (browse_ >= 0) {
        const HistoryEntry& h = hist[static_cast<std::size_t>(std::min(browse_, static_cast<int>(hist.size()) - 1))];
        const char* f4 = h.collection ? (h.collection->value.isList() ? "ListAns" : h.collection->value.type == ValueType::Vector ? "VctAns" : "MatAns") : "PERIOD";
        const char* keys[6] = {"MODIF", "COPIA", "RIS", f4, shift ? "ab/c" : "S\xE2\x87\x94" "D", "ELIM"};
        ui::softKeys(c, keys);
    } else {
        const char* normal[6] = {"a/b", "\xE2\x88\x9A\xE2\x96\xA1", "\xE2\x96\xA1^", "MATH", "S\xE2\x87\x94" "D", "Ans"};
        const char* shifted[6] = {"a b/c", "\xE2\x96\xA1\xC2\xB2", "\xCF\x80", "\xE2\x86\x92", "ab/c\xE2\x87\x94" "d/c", "\xCE\xB8"};
        ui::softKeys(c, shift ? shifted : normal);
    }
    menu_.draw(c);
}
} // namespace fx::app
