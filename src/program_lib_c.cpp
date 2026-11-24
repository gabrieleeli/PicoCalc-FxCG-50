// Libreria standard del C (v1.20.0, anche per il C++ con <cXXX> e std::):
// <stdio.h> (printf e scanf con le loro famiglie, console, FILE confinati
// nella cartella del programma), <stdlib.h> (heap controllato, conversioni,
// rand della norma, qsort, bsearch, exit), <string.h> con il controllo dei
// limiti, <ctype.h>, <math.h>, <time.h> (tempo dall'accensione), assert.
#include "program_lib.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace fx::prog {
using namespace lib;

namespace {
// ================================================================ char* del programma
struct CharRef { ArrayObj* a{nullptr}; std::uint32_t idx{0}; };
CharRef charArray(Vm& vm, const Value& p, const char* what) {
    Value x = p;
    if (x.isNull()) vm.fault(FaultKind::NullPointer, std::string(what) + ": puntatore NULL");
    if (x.k == VK::Ref) x = Value::ptr(x.o, 0);
    if (x.k == VK::Ptr && x.f == 2) vm.fault(FaultKind::Generic, std::string(what) + ": serve un array di char, non l'indirizzo di una variabile");
    if (x.k != VK::Ptr || !x.o) vm.fault(FaultKind::Generic, std::string(what) + ": atteso un char*");
    if (x.o->dead()) vm.fault(FaultKind::DeadPointer, std::string(what) + ": memoria gia' liberata");
    if (x.o->kind == OK::StrB) vm.fault(FaultKind::ReadOnly, std::string(what) + ": c_str() e' di sola lettura");
    if (x.o->kind != OK::Array) vm.fault(FaultKind::Generic, std::string(what) + ": atteso un array di char");
    ArrayObj* a = static_cast<ArrayObj*>(x.o);
    if (a->ek == VK::Void) {
        // malloc senza conversione: diventa un array di char
        const Value typed = vm.castTo(x, vm.program().charPtr);
        (void)typed;
    }
    if (a->ek != VK::C8 && a->ek != VK::I8 && a->ek != VK::U8) vm.fault(FaultKind::Generic, std::string(what) + ": atteso un array di char");
    CharRef r;
    r.a = a;
    r.idx = x.idx == Whole ? 0 : x.idx;
    if (r.idx > a->count) vm.fault(FaultKind::Index, std::string(what) + ": puntatore oltre la fine dell'array");
    return r;
}
// Scrive s e '\0' in dst (al massimo `limit` byte compreso '\0').
void putCStr(Vm& vm, const Value& dst, std::string_view s, const char* what) {
    CharRef r = charArray(vm, dst, what);
    if (r.a->flags & FReadOnly) vm.fault(FaultKind::ReadOnly, std::string(what) + ": scrittura su una stringa letterale");
    if (static_cast<std::uint64_t>(r.idx) + s.size() + 1 > r.a->count)
        vm.fault(FaultKind::Index, std::string(what) + ": " + std::to_string(s.size() + 1) + " byte in un array di " + std::to_string(r.a->count - r.idx) + " (oltre la fine)");
    std::memcpy(r.a->data + r.idx, s.data(), s.size());
    r.a->data[r.idx + s.size()] = 0;
}
std::string cstr(Vm& vm, const NativeCall& c, int i) { return S(vm, c, i); }
Value ptrAt(const CharRef& r, std::size_t off) { return Value::ptr(r.a, static_cast<std::uint32_t>(r.idx + off)); }

// ================================================================ printf
void cPrintf(Vm& vm, NativeCall& c) {
    int used = 0;
    const std::string out = formatPrintf(vm, cstr(vm, c, 0), c.args + 1, c.argc - 1, used);
    vm.host().print(out);
    c.ret = Value::i32(static_cast<std::int64_t>(out.size()));
}
void cFprintf(Vm& vm, NativeCall& c) {
    FileObj* f = fileArg(vm, c.args[0]);
    int used = 0;
    const std::string out = formatPrintf(vm, cstr(vm, c, 1), c.args + 2, c.argc - 2, used);
    filePut(vm, f, out);
    c.ret = Value::i32(f->fail ? -1 : static_cast<std::int64_t>(out.size()));
}
void cSprintf(Vm& vm, NativeCall& c) {
    int used = 0;
    const std::string out = formatPrintf(vm, cstr(vm, c, 1), c.args + 2, c.argc - 2, used);
    putCStr(vm, c.args[0], out, "sprintf");
    c.ret = Value::i32(static_cast<std::int64_t>(out.size()));
}
void cSnprintf(Vm& vm, NativeCall& c) {
    int used = 0;
    const std::string out = formatPrintf(vm, cstr(vm, c, 2), c.args + 3, c.argc - 3, used);
    const std::int64_t n = I(c, 1);
    if (n > 0) {
        const std::size_t keep = std::min<std::size_t>(out.size(), static_cast<std::size_t>(n - 1));
        putCStr(vm, c.args[0], std::string_view(out).substr(0, keep), "snprintf");
    }
    c.ret = Value::i32(static_cast<std::int64_t>(out.size()));
}
void cPuts(Vm& vm, NativeCall& c) { vm.host().print(cstr(vm, c, 0) + "\n"); c.ret = Value::i32(1); }
void cPutchar(Vm& vm, NativeCall& c) { const int ch = static_cast<unsigned char>(I(c, 0)); vm.host().print(std::string(1, static_cast<char>(ch))); c.ret = Value::i32(ch); }
void cGetchar(Vm& vm, NativeCall& c) { c.ret = Value::i32(conGet(vm)); }
void cGets(Vm& vm, NativeCall&) { vm.fault(FaultKind::NotSupported, "gets non e' supportata (non controlla la lunghezza): usa fgets(buf, n, stdin)"); }

// ================================================================ scanf
struct Src {
    explicit Src(Vm& v) : vm(v) {}
    Vm& vm;
    int mode{0};                 // 0 console, 1 file, 2 stringa
    FileObj* f{nullptr};
    std::string s;
    std::size_t pos{0};
    std::size_t consumed{0};
    int peek() {
        if (mode == 0) return conPeek(vm);
        if (mode == 1) return filePeek(vm, f);
        return pos < s.size() ? static_cast<unsigned char>(s[pos]) : -1;
    }
    int get() {
        int ch;
        if (mode == 0) ch = conGet(vm);
        else if (mode == 1) ch = fileGet(vm, f);
        else ch = pos < s.size() ? static_cast<unsigned char>(s[pos++]) : -1;
        if (ch >= 0) ++consumed;
        return ch;
    }
};
void storeNum(Vm& vm, const Value& ptr, const Value& v, VK implied) {
    if (ptr.isNull()) vm.fault(FaultKind::NullPointer, "scanf: argomento NULL (manca la &?)");
    if (ptr.k != VK::Ptr) vm.fault(FaultKind::Argument, "scanf: l'argomento deve essere un indirizzo (&variabile)");
    const Value cur = vm.loadPtr(ptr);
    const VK k = isNum(cur.k) ? cur.k : implied;
    vm.storePtr(ptr, vm.convertTo(v, k));
}
int doScanf(Vm& vm, Src& in, const std::string& fmt, const Value* args, int argc) {
    int assigned = 0, ai = 0;
    bool anyInput = false;
    auto nextArg = [&](char conv) -> const Value& {
        if (ai >= argc) vm.fault(FaultKind::Argument, std::string("scanf: manca l'argomento per %") + conv);
        return args[ai++];
    };
    auto skipWs = [&] { int ch = in.peek(); while (ch >= 0 && std::isspace(ch)) { in.get(); ch = in.peek(); } };
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        const char fc = fmt[i];
        if (std::isspace(static_cast<unsigned char>(fc))) { skipWs(); continue; }
        if (fc != '%') {
            const int ch = in.peek();
            if (ch < 0) return assigned == 0 && !anyInput ? -1 : assigned;
            if (ch != static_cast<unsigned char>(fc)) return assigned;
            in.get();
            anyInput = true;
            continue;
        }
        if (++i >= fmt.size()) vm.fault(FaultKind::Format, "scanf: % alla fine del formato");
        if (fmt[i] == '%') { skipWs(); if (in.peek() != '%') return assigned; in.get(); continue; }
        bool suppress = false;
        if (fmt[i] == '*') { suppress = true; ++i; }
        std::size_t width = 0;
        while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) width = width * 10 + static_cast<std::size_t>(fmt[i++] - '0');
        int lmod = 0;                                          // 1 h, 2 hh, 3 l, 4 ll, 5 L
        while (i < fmt.size() && std::strchr("hlLqjzt", fmt[i])) {
            const char m = fmt[i++];
            if (m == 'h') lmod = lmod == 1 ? 2 : 1;
            else if (m == 'l') lmod = lmod == 3 ? 4 : 3;
            else if (m == 'L' || m == 'q') lmod = 5;
            else lmod = 4;
        }
        if (i >= fmt.size()) vm.fault(FaultKind::Format, "scanf: conversione incompleta");
        const char conv = fmt[i];
        if (conv == 'n') {
            if (!suppress) storeNum(vm, nextArg('n'), Value::i32(static_cast<std::int64_t>(in.consumed)), VK::I32);
            continue;
        }
        if (conv != 'c' && conv != '[') skipWs();
        if (in.peek() < 0) return assigned == 0 && !anyInput ? -1 : assigned;
        const std::size_t lim = width ? width : SIZE_MAX;
        switch (conv) {
        case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': {
            int base = conv == 'd' || conv == 'u' ? 10 : conv == 'o' ? 8 : conv == 'i' ? 0 : 16;
            std::string t;
            int ch = in.peek();
            if ((ch == '+' || ch == '-') && t.size() < lim) { t += static_cast<char>(in.get()); ch = in.peek(); }
            if ((base == 0 || base == 16) && ch == '0' && t.size() < lim) {
                t += static_cast<char>(in.get());
                ch = in.peek();
                if ((ch == 'x' || ch == 'X') && t.size() < lim) { t += static_cast<char>(in.get()); ch = in.peek(); base = 16; }
                else if (base == 0) base = 8;
            }
            if (base == 0) base = 10;
            auto digit = [&](int x) { if (x < 0) return false; if (base == 16) return std::isxdigit(x) != 0; if (base == 8) return x >= '0' && x <= '7'; return std::isdigit(x) != 0; };
            while (digit(ch) && t.size() < lim) { t += static_cast<char>(in.get()); ch = in.peek(); }
            std::int64_t v = 0;
            bool ovf = false;
            const std::size_t n = parseInt(t, 0, base, v, ovf);
            if (n == 0) return assigned;
            anyInput = true;
            if (!suppress) {
                const VK implied = lmod >= 3 ? (conv == 'u' ? VK::U64 : VK::I64) : lmod == 1 ? VK::I16 : lmod == 2 ? VK::I8 : (conv == 'u' || conv == 'x' || conv == 'X' || conv == 'o' ? VK::U32 : VK::I32);
                storeNum(vm, nextArg(conv), Value::i64(v), implied);
                ++assigned;
            }
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            std::string t;
            int ch = in.peek();
            auto take = [&] { t += static_cast<char>(in.get()); ch = in.peek(); };
            if ((ch == '+' || ch == '-') && t.size() < lim) take();
            // inf / nan
            if (ch == 'i' || ch == 'I' || ch == 'n' || ch == 'N') {
                while (ch >= 0 && std::isalpha(ch) && t.size() < lim) take();
            } else {
                while (ch >= 0 && std::isdigit(ch) && t.size() < lim) take();
                if (ch == '.' && t.size() < lim) { take(); while (ch >= 0 && std::isdigit(ch) && t.size() < lim) take(); }
                if ((ch == 'e' || ch == 'E') && t.size() < lim) {
                    take();
                    if ((ch == '+' || ch == '-') && t.size() < lim) take();
                    while (ch >= 0 && std::isdigit(ch) && t.size() < lim) take();
                }
            }
            double d = 0;
            if (parseDouble(t, 0, d) == 0) return assigned;
            anyInput = true;
            if (!suppress) { storeNum(vm, nextArg(conv), Value::f64(d), lmod == 3 || lmod == 5 ? VK::F64 : VK::F32); ++assigned; }
            break;
        }
        case 'c': {
            const std::size_t n = width ? width : 1;
            std::string t;
            for (std::size_t k = 0; k < n; ++k) { const int ch = in.get(); if (ch < 0) break; t += static_cast<char>(ch); }
            if (t.empty()) return assigned == 0 && !anyInput ? -1 : assigned;
            anyInput = true;
            if (!suppress) {
                const Value& p = nextArg('c');
                if (n == 1) storeNum(vm, p, Value::ofInt(VK::C8, static_cast<signed char>(t[0])), VK::C8);
                else {
                    CharRef r = charArray(vm, p, "scanf %c");
                    if (r.idx + t.size() > r.a->count) vm.fault(FaultKind::Index, "scanf: oltre la fine dell'array");
                    std::memcpy(r.a->data + r.idx, t.data(), t.size());
                }
                ++assigned;
            }
            break;
        }
        case 's': case '[': {
            std::string set;
            bool negate = false;
            if (conv == '[') {
                std::size_t j = i + 1;
                if (j < fmt.size() && fmt[j] == '^') { negate = true; ++j; }
                if (j < fmt.size() && fmt[j] == ']') { set += ']'; ++j; }
                while (j < fmt.size() && fmt[j] != ']') {
                    if (j + 2 < fmt.size() && fmt[j + 1] == '-' && fmt[j + 2] != ']') { for (int x = static_cast<unsigned char>(fmt[j]); x <= static_cast<unsigned char>(fmt[j + 2]); ++x) set += static_cast<char>(x); j += 3; }
                    else set += fmt[j++];
                }
                if (j >= fmt.size()) vm.fault(FaultKind::Format, "scanf: %[ senza ]");
                i = j;
            }
            auto ok = [&](int ch) {
                if (ch < 0) return false;
                if (conv == 's') return !std::isspace(ch);
                const bool in2 = set.find(static_cast<char>(ch)) != std::string::npos;
                return negate ? !in2 : in2;
            };
            std::string t;
            int ch = in.peek();
            while (ok(ch) && t.size() < lim) { t += static_cast<char>(in.get()); ch = in.peek(); }
            if (t.empty()) return assigned;
            anyInput = true;
            if (!suppress) { putCStr(vm, nextArg(conv), t, "scanf %s"); ++assigned; }
            break;
        }
        case 'p': vm.fault(FaultKind::NotSupported, "scanf %p non e' supportato");
        default: vm.fault(FaultKind::Format, std::string("scanf: conversione %") + conv + " sconosciuta");
        }
    }
    return assigned;
}
void cScanf(Vm& vm, NativeCall& c) { Src in(vm); c.ret = Value::i32(doScanf(vm, in, cstr(vm, c, 0), c.args + 1, c.argc - 1)); }
void cSscanf(Vm& vm, NativeCall& c) { Src in(vm); in.mode = 2; in.s = cstr(vm, c, 0); c.ret = Value::i32(doScanf(vm, in, cstr(vm, c, 1), c.args + 2, c.argc - 2)); }
void cFscanf(Vm& vm, NativeCall& c) {
    Src in(vm);
    in.f = fileArg(vm, c.args[0]);
    in.mode = in.f->console ? 0 : 1;
    c.ret = Value::i32(doScanf(vm, in, cstr(vm, c, 1), c.args + 2, c.argc - 2));
}

// ================================================================ FILE
void setIoError(Vm& vm, const std::string& why) { vm.lastIoError = why; vm.lastErrno = 2; }
void cFopen(Vm& vm, NativeCall& c) {
    FileObj* f = vm.newFile();
    vm.root(f);
    std::string why;
    std::string mode = cstr(vm, c, 1);
    mode.erase(std::remove(mode.begin(), mode.end(), 'b'), mode.end());
    mode.erase(std::remove(mode.begin(), mode.end(), 't'), mode.end());
    if (!fileOpen(vm, f, cstr(vm, c, 0), mode, why)) { setIoError(vm, why); c.ret = Value::null(); return; }
    c.ret = Value::ptr(f, Whole);
}
void cFclose(Vm& vm, NativeCall& c) {
    FileObj* f = fileArg(vm, c.args[0]);
    if (f->console) { c.ret = Value::i32(0); return; }
    if (!f->open) vm.fault(FaultKind::Generic, "fclose di un file gia' chiuso");
    const bool ok = fileClose(vm, f);
    if (!ok) setIoError(vm, "scrittura non riuscita (SD piena o rimossa)");
    c.ret = Value::i32(ok ? 0 : -1);
}
FileObj* openFile(Vm& vm, const Value& v, const char* what) {
    FileObj* f = fileArg(vm, v);
    if (!f->open && !f->console) vm.fault(FaultKind::Generic, std::string(what) + " su un file chiuso");
    return f;
}
void cFgets(Vm& vm, NativeCall& c) {
    FileObj* f = openFile(vm, c.args[2], "fgets");
    const std::int64_t n = I(c, 1);
    if (n <= 0) { c.ret = Value::null(); return; }
    std::string t;
    while (static_cast<std::int64_t>(t.size()) < n - 1) {
        const int ch = fileGet(vm, f);
        if (ch < 0) break;
        t += static_cast<char>(ch);
        if (ch == '\n') break;
    }
    if (t.empty()) { c.ret = Value::null(); return; }
    putCStr(vm, c.args[0], t, "fgets");
    c.ret = c.args[0];
}
void cFputs(Vm& vm, NativeCall& c) { FileObj* f = openFile(vm, c.args[1], "fputs"); filePut(vm, f, cstr(vm, c, 0)); c.ret = Value::i32(f->fail ? -1 : 1); }
void cFputc(Vm& vm, NativeCall& c) {
    FileObj* f = openFile(vm, c.args[1], "fputc");
    const char ch = static_cast<char>(I(c, 0));
    filePut(vm, f, std::string_view(&ch, 1));
    c.ret = Value::i32(f->fail ? -1 : static_cast<unsigned char>(ch));
}
void cFgetc(Vm& vm, NativeCall& c) { FileObj* f = openFile(vm, c.args[0], "fgetc"); c.ret = Value::i32(fileGet(vm, f)); }
void cUngetc(Vm& vm, NativeCall& c) { FileObj* f = openFile(vm, c.args[1], "ungetc"); fileUnget(vm, f, static_cast<int>(I(c, 0))); c.ret = c.args[0]; }
void cFeof(Vm& vm, NativeCall& c) {
    FileObj* f = fileArg(vm, c.args[0]);
    c.ret = Value::i32(f->eof ? 1 : 0);
}
void cFerror(Vm& vm, NativeCall& c) { FileObj* f = fileArg(vm, c.args[0]); c.ret = Value::i32(f->error || f->fail ? 1 : 0); }
void cClearerr(Vm& vm, NativeCall& c) { FileObj* f = fileArg(vm, c.args[0]); f->eof = f->error = f->fail = false; }
void cFflush(Vm&, NativeCall& c) { c.ret = Value::i32(0); }
void cRewind(Vm& vm, NativeCall& c) {
    FileObj* f = openFile(vm, c.args[0], "rewind");
    f->eof = false;
    if (f->memory || (f->mode & (2 | 4 | 8))) { f->pos = 0; return; }
    std::string why;
    const std::string path = f->path;
    files::FileSystem* fs = vm.host().files();
    if (!fs) return;
    f->reader.reset();
    if (fs->openRead(path, f->reader) != files::Status::Ok) { f->error = true; return; }
    f->buf.clear();
    f->pos = 0;
    f->readerEnd = false;
    f->filePos = 0;
    f->unget = -1;
}
void cFtell(Vm& vm, NativeCall& c) {
    FileObj* f = openFile(vm, c.args[0], "ftell");
    if (f->memory || (f->mode & (2 | 4 | 8))) { c.ret = Value::i64(static_cast<std::int64_t>(f->mode & 1 ? f->pos : f->buf.size())); return; }
    c.ret = Value::i64(static_cast<std::int64_t>(f->filePos - (f->buf.size() - f->pos)));
}
void cFseek(Vm& vm, NativeCall& c) {
    FileObj* f = openFile(vm, c.args[0], "fseek");
    const std::int64_t off = I(c, 1), whence = I(c, 2);
    f->eof = false;
    if (f->memory || (f->mode & 8)) {
        std::int64_t base = whence == 0 ? 0 : whence == 1 ? static_cast<std::int64_t>(f->pos) : static_cast<std::int64_t>(f->buf.size());
        const std::int64_t np = base + off;
        if (np < 0 || np > static_cast<std::int64_t>(f->buf.size())) { c.ret = Value::i32(-1); return; }
        f->pos = static_cast<std::size_t>(np);
        c.ret = Value::i32(0);
        return;
    }
    if (f->mode & (2 | 4)) { c.ret = Value::i32(whence == 2 && off == 0 ? 0 : -1); return; }
    // lettura: dall'inizio e avanti di off byte
    std::int64_t target = off;
    if (whence == 1) {
        target = static_cast<std::int64_t>(f->filePos - (f->buf.size() - f->pos)) + off;
    } else if (whence == 2) {
        files::Entry e;
        files::FileSystem* fs = vm.host().files();
        if (!fs || fs->stat(f->path, e) != files::Status::Ok) { c.ret = Value::i32(-1); return; }
        target = static_cast<std::int64_t>(e.size) + off;
    }
    if (target < 0) { c.ret = Value::i32(-1); return; }
    NativeCall rw;
    rw.args = c.args;
    rw.argc = 1;
    cRewind(vm, rw);
    for (std::int64_t k = 0; k < target; ++k) if (fileGet(vm, f) < 0) break;
    f->eof = false;
    c.ret = Value::i32(0);
}
void cRemove(Vm& vm, NativeCall& c) {
    std::string why;
    const bool ok = fileRemove(vm, cstr(vm, c, 0), why);
    if (!ok) setIoError(vm, why);
    c.ret = Value::i32(ok ? 0 : -1);
}
void cRename(Vm& vm, NativeCall& c) {
    std::string why;
    const bool ok = fileRename(vm, cstr(vm, c, 0), cstr(vm, c, 1), why);
    if (!ok) setIoError(vm, why);
    c.ret = Value::i32(ok ? 0 : -1);
}
void cPerror(Vm& vm, NativeCall& c) {
    std::string s = c.args[0].isNull() ? std::string() : cstr(vm, c, 0);
    const std::string why = vm.lastIoError.empty() ? "nessun errore" : vm.lastIoError;
    vm.host().print((s.empty() ? std::string() : s + ": ") + why + "\n");
}

// ================================================================ stdlib
void cAbs(Vm&, NativeCall& c) { const std::int64_t v = I(c, 0); c.ret = Value::i32(v < 0 ? -v : v); }
void cLabs(Vm&, NativeCall& c) { const std::int64_t v = I(c, 0); c.ret = Value::i64(v < 0 ? static_cast<std::int64_t>(0 - static_cast<std::uint64_t>(v)) : v); }
void cAtoi(Vm& vm, NativeCall& c) { std::int64_t v = 0; bool o = false; parseInt(cstr(vm, c, 0), 0, 10, v, o); c.ret = Value::i32(v); }
void cAtol(Vm& vm, NativeCall& c) { std::int64_t v = 0; bool o = false; parseInt(cstr(vm, c, 0), 0, 10, v, o); c.ret = Value::i64(v); }
void cAtof(Vm& vm, NativeCall& c) { double d = 0; const std::string s = cstr(vm, c, 0); std::size_t i = 0; while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; parseDouble(s, i, d); c.ret = Value::f64(d); }
void storeEnd(Vm& vm, const Value& endp, const Value& src, std::size_t used) {
    if (endp.isNull()) return;
    Value p = src;
    if (p.k == VK::Ref) p = Value::ptr(p.o, 0);
    if (p.k == VK::Ptr && p.f != 2) p.idx += static_cast<std::uint32_t>(used);
    vm.storePtr(endp, p);
}
template <VK K> void cStrtol(Vm& vm, NativeCall& c) {
    const std::string s = cstr(vm, c, 0);
    std::int64_t v = 0;
    bool ovf = false;
    const std::size_t n = parseInt(s, 0, static_cast<int>(I(c, 2)), v, ovf);
    if (ovf) { v = K == VK::U64 ? -1 : (s.find('-') != std::string::npos ? INT64_MIN : INT64_MAX); vm.lastErrno = 34; }
    storeEnd(vm, c.args[1], c.args[0], n);
    c.ret = Value::ofInt(K, v);
}
void cStrtod(Vm& vm, NativeCall& c) {
    const std::string s = cstr(vm, c, 0);
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    double d = 0;
    const std::size_t n = parseDouble(s, i, d);
    storeEnd(vm, c.args[1], c.args[0], n ? i + n : 0);
    c.ret = Value::f64(d);
}
void cStrtof(Vm& vm, NativeCall& c) { cStrtod(vm, c); c.ret = Value::f32(c.ret.d); }
void cRand(Vm& vm, NativeCall& c) {
    // generatore dell'esempio della norma C11 (7.22.2.2), RAND_MAX 32767
    vm.randState = vm.randState * 1103515245u + 12345u;
    c.ret = Value::i32(static_cast<std::int64_t>((vm.randState / 65536u) % 32768u));
}
void cSrand(Vm& vm, NativeCall& c) { vm.randState = static_cast<std::uint32_t>(I(c, 0)); vm.randSeeded = true; }
void cExit(Vm& vm, NativeCall& c) { (void)vm; throw ProgramExit{static_cast<int>(I(c, 0))}; }
void cAbort(Vm& vm, NativeCall&) { vm.fatal("Argument ERROR: abort()"); }
void cSystem(Vm& vm, NativeCall&) { vm.fault(FaultKind::NotSupported, "system() non e' disponibile sulla PicoCalc"); }
void cGetenv(Vm&, NativeCall& c) { c.ret = Value::null(); }
void cMalloc(Vm& vm, NativeCall& c) {
    const std::int64_t n = I(c, 0);
    if (n < 0 || static_cast<std::uint64_t>(n) > vm.heap().budget()) { c.ret = Value::null(); return; }
    try {
        ArrayObj* a = vm.newArray(vm.program().prim[static_cast<int>(TK::Void)], static_cast<std::uint32_t>(n), true);
        a->flags |= FExplicit;
        c.ret = Value::ptr(a, 0);
    } catch (std::bad_alloc&) {
        c.ret = Value::null();
    } catch (ProgramThrow&) {
        c.ret = Value::null();
    }
}
void cCalloc(Vm& vm, NativeCall& c) {
    const std::int64_t n = I(c, 0), sz = I(c, 1);
    if (n < 0 || sz < 0 || (sz && static_cast<std::uint64_t>(n) > vm.heap().budget() / static_cast<std::uint64_t>(sz))) { c.ret = Value::null(); return; }
    NativeCall m = c;
    Value one = Value::i64(n * sz);
    m.args = &one;
    m.argc = 1;
    cMalloc(vm, m);
    c.ret = m.ret;
}
void cFree(Vm& vm, NativeCall& c) {
    const Value v = c.args[0];
    if (v.isNull()) return;
    if (v.k == VK::Ptr && v.f == 2) vm.fault(FaultKind::Generic, "free di una variabile locale (non e' memoria di malloc)");
    Obj* o = v.o;
    if (!o) return;
    if (o->dead()) vm.fault(FaultKind::DeadPointer, "free di memoria gia' liberata (doppio free)");
    if (!(o->flags & FExplicit)) vm.fault(FaultKind::Generic, "free di memoria non allocata con malloc");
    if (v.k == VK::Ptr && v.idx != 0 && v.idx != Whole) vm.fault(FaultKind::Generic, "free di un puntatore che non punta all'inizio del blocco");
    vm.killValue(Value::ref(o));
}
void cRealloc(Vm& vm, NativeCall& c) {
    const Value old = c.args[0];
    const std::int64_t n = I(c, 1);
    if (old.isNull()) { NativeCall m = c; m.args = c.args + 1; m.argc = 1; cMalloc(vm, m); c.ret = m.ret; return; }
    if (n == 0) { cFree(vm, c); c.ret = Value::null(); return; }
    if (old.k != VK::Ptr || old.f == 2 || !old.o || old.o->kind != OK::Array) vm.fault(FaultKind::Generic, "realloc di memoria non allocata con malloc");
    ArrayObj* a = static_cast<ArrayObj*>(old.o);
    if (a->dead()) vm.fault(FaultKind::DeadPointer, "realloc di memoria gia' liberata");
    if (!(a->flags & FExplicit)) vm.fault(FaultKind::Generic, "realloc di memoria non allocata con malloc");
    if (n < 0 || static_cast<std::uint64_t>(n) > vm.heap().budget()) { c.ret = Value::null(); return; }
    ArrayObj* b = nullptr;
    try {
        if (a->ek == VK::Void) {
            b = vm.newArray(vm.program().prim[static_cast<int>(TK::Void)], static_cast<std::uint32_t>(n), true);
        } else {
            const std::int64_t es = vm.sizeOfType(a->elem);
            if (es <= 0 || n % es != 0) vm.fault(FaultKind::Argument, "realloc di " + std::to_string(n) + " byte non e' un multiplo di sizeof(" + (a->elem ? a->elem->name() : std::string("?")) + ")");
            b = vm.newArray(a->elem, static_cast<std::uint32_t>(n / es), true);
            const std::uint32_t k = std::min(a->count, b->count);
            for (std::uint32_t i = 0; i < k; ++i) b->set(i, a->get(i));
        }
    } catch (std::bad_alloc&) {
        c.ret = Value::null();
        return;
    }
    b->flags |= FExplicit;
    vm.root(b);
    // il vecchio blocco e' liberato senza distruggere gli elementi spostati
    a->flags |= FDead;
    a->flags &= static_cast<std::uint8_t>(~FExplicit);
    if (a->data) { vm.heap().free(a->data); a->data = nullptr; }
    a->count = 0;
    c.ret = Value::ptr(b, 0);
}
void cQsort(Vm& vm, NativeCall& c) {
    const Value base = c.args[0];
    const std::int64_t n = I(c, 1);
    if (n <= 0) return;
    if (base.k != VK::Ptr || base.f == 2 || !base.o || base.o->kind != OK::Array) vm.fault(FaultKind::Argument, "qsort: serve un array");
    ArrayObj* a = static_cast<ArrayObj*>(base.o);
    const std::uint32_t start = base.idx == Whole ? 0 : base.idx;
    if (static_cast<std::uint64_t>(start) + static_cast<std::uint64_t>(n) > a->count) vm.fault(FaultKind::Index, "qsort: " + std::to_string(n) + " elementi oltre la fine dell'array");
    const std::int64_t es = vm.sizeOfType(a->elem);
    if (I(c, 2) != es) vm.fault(FaultKind::Argument, "qsort: la dimensione dell'elemento (" + std::to_string(I(c, 2)) + ") non e' sizeof dell'array (" + std::to_string(es) + ")");
    const Value cmp = c.args[3];
    std::vector<Value> perm;
    perm.reserve(static_cast<std::size_t>(n));
    for (std::int64_t i = 0; i < n; ++i) perm.push_back(Value::i64(i));
    stableSort(vm, perm, [&](const Value& x, const Value& y) {
        Value args[2] = {Value::ptr(a, start + static_cast<std::uint32_t>(x.i)), Value::ptr(a, start + static_cast<std::uint32_t>(y.i))};
        return vm.invoke(cmp, args, 2).integer() < 0;
    });
    std::vector<Value> vals;
    vals.reserve(perm.size());
    for (const Value& p : perm) vals.push_back(a->get(start + static_cast<std::uint32_t>(p.i)));
    VecObj* keep = vm.newVec(nullptr);
    vm.root(keep);
    keep->items.assign(vals.begin(), vals.end());
    for (std::size_t i = 0; i < vals.size(); ++i) a->set(start + static_cast<std::uint32_t>(i), vals[i]);
}
void cBsearch(Vm& vm, NativeCall& c) {
    const Value key = c.args[0], base = c.args[1];
    const std::int64_t n = I(c, 2);
    const Value cmp = c.args[4];
    if (n <= 0) { c.ret = Value::null(); return; }
    if (base.k != VK::Ptr || base.f == 2 || !base.o || base.o->kind != OK::Array) vm.fault(FaultKind::Argument, "bsearch: serve un array");
    ArrayObj* a = static_cast<ArrayObj*>(base.o);
    const std::uint32_t start = base.idx == Whole ? 0 : base.idx;
    if (static_cast<std::uint64_t>(start) + static_cast<std::uint64_t>(n) > a->count) vm.fault(FaultKind::Index, "bsearch: oltre la fine dell'array");
    std::int64_t lo = 0, hi = n - 1;
    while (lo <= hi) {
        const std::int64_t mid = lo + (hi - lo) / 2;
        Value args[2] = {key, Value::ptr(a, start + static_cast<std::uint32_t>(mid))};
        const std::int64_t r = vm.invoke(cmp, args, 2).integer();
        if (r == 0) { c.ret = args[1]; return; }
        if (r < 0) hi = mid - 1; else lo = mid + 1;
    }
    c.ret = Value::null();
}

// ================================================================ string.h
void cStrlen(Vm& vm, NativeCall& c) { c.ret = Value::ofInt(VK::U64, static_cast<std::int64_t>(cstr(vm, c, 0).size())); }
void cStrcpy(Vm& vm, NativeCall& c) { putCStr(vm, c.args[0], cstr(vm, c, 1), "strcpy"); c.ret = c.args[0]; }
void cStrncpy(Vm& vm, NativeCall& c) {
    const std::string src = cstr(vm, c, 1);
    const std::int64_t n = I(c, 2);
    CharRef r = charArray(vm, c.args[0], "strncpy");
    if (r.a->flags & FReadOnly) vm.fault(FaultKind::ReadOnly, "strncpy: scrittura su una stringa letterale");
    if (static_cast<std::uint64_t>(r.idx) + static_cast<std::uint64_t>(n) > r.a->count) vm.fault(FaultKind::Index, "strncpy: " + std::to_string(n) + " byte oltre la fine dell'array");
    for (std::int64_t i = 0; i < n; ++i) r.a->data[r.idx + static_cast<std::size_t>(i)] = static_cast<std::size_t>(i) < src.size() ? static_cast<std::uint8_t>(src[static_cast<std::size_t>(i)]) : 0;
    c.ret = c.args[0];
}
void cStrcat(Vm& vm, NativeCall& c) { putCStr(vm, c.args[0], cstr(vm, c, 0) + cstr(vm, c, 1), "strcat"); c.ret = c.args[0]; }
void cStrncat(Vm& vm, NativeCall& c) {
    const std::string add = cstr(vm, c, 1).substr(0, static_cast<std::size_t>(std::max<std::int64_t>(0, I(c, 2))));
    putCStr(vm, c.args[0], cstr(vm, c, 0) + add, "strncat");
    c.ret = c.args[0];
}
int sgn(int x) { return x < 0 ? -1 : x > 0 ? 1 : 0; }
void cStrcmp(Vm& vm, NativeCall& c) { c.ret = Value::i32(sgn(std::strcmp(cstr(vm, c, 0).c_str(), cstr(vm, c, 1).c_str()))); }
void cStrncmp(Vm& vm, NativeCall& c) { c.ret = Value::i32(sgn(std::strncmp(cstr(vm, c, 0).c_str(), cstr(vm, c, 1).c_str(), static_cast<std::size_t>(std::max<std::int64_t>(0, I(c, 2)))))); }
void cStrchr(Vm& vm, NativeCall& c) {
    const std::string s = cstr(vm, c, 0);
    const char ch = static_cast<char>(I(c, 1));
    CharRef r = charArray(vm, c.args[0], "strchr");
    const std::size_t p = ch == 0 ? s.size() : s.find(ch);
    c.ret = p == std::string::npos ? Value::null() : ptrAt(r, p);
}
void cStrrchr(Vm& vm, NativeCall& c) {
    const std::string s = cstr(vm, c, 0);
    const char ch = static_cast<char>(I(c, 1));
    CharRef r = charArray(vm, c.args[0], "strrchr");
    const std::size_t p = ch == 0 ? s.size() : s.rfind(ch);
    c.ret = p == std::string::npos ? Value::null() : ptrAt(r, p);
}
void cStrstr(Vm& vm, NativeCall& c) {
    const std::string s = cstr(vm, c, 0);
    CharRef r = charArray(vm, c.args[0], "strstr");
    const std::size_t p = s.find(cstr(vm, c, 1));
    c.ret = p == std::string::npos ? Value::null() : ptrAt(r, p);
}
void cStrspn(Vm& vm, NativeCall& c) { c.ret = Value::ofInt(VK::U64, static_cast<std::int64_t>(std::strspn(cstr(vm, c, 0).c_str(), cstr(vm, c, 1).c_str()))); }
void cStrcspn(Vm& vm, NativeCall& c) { c.ret = Value::ofInt(VK::U64, static_cast<std::int64_t>(std::strcspn(cstr(vm, c, 0).c_str(), cstr(vm, c, 1).c_str()))); }
void cStrpbrk(Vm& vm, NativeCall& c) {
    const std::string s = cstr(vm, c, 0);
    CharRef r = charArray(vm, c.args[0], "strpbrk");
    const std::size_t p = s.find_first_of(cstr(vm, c, 1));
    c.ret = p == std::string::npos ? Value::null() : ptrAt(r, p);
}
void cStrtok(Vm& vm, NativeCall& c) {
    const std::string delim = cstr(vm, c, 1);
    Value start = c.args[0];
    if (start.isNull()) start = vm.strtokState;
    if (start.isNull()) { c.ret = Value::null(); return; }
    CharRef r = charArray(vm, start, "strtok");
    if (r.a->flags & FReadOnly) vm.fault(FaultKind::ReadOnly, "strtok su una stringa letterale (copiala in un array)");
    std::uint32_t i = r.idx;
    auto at = [&](std::uint32_t k) -> char { if (k >= r.a->count) vm.fault(FaultKind::Index, "strtok: stringa senza '\\0'"); return static_cast<char>(r.a->data[k]); };
    while (at(i) && delim.find(at(i)) != std::string::npos) ++i;
    if (!at(i)) { vm.strtokState = Value::null(); c.ret = Value::null(); return; }
    const std::uint32_t tok = i;
    while (at(i) && delim.find(at(i)) == std::string::npos) ++i;
    if (at(i)) { r.a->data[i] = 0; vm.strtokState = Value::ptr(r.a, i + 1); }
    else vm.strtokState = Value::null();
    c.ret = Value::ptr(r.a, tok);
}
void cStrdup(Vm& vm, NativeCall& c) {
    const std::string s = cstr(vm, c, 0);
    ArrayObj* a = vm.newCString(s, false);
    a->flags |= FExplicit;
    c.ret = Value::ptr(a, 0);
}
void cStrerror(Vm& vm, NativeCall& c) { c.ret = Value::ptr(vm.newCString(I(c, 0) == 0 ? "Success" : vm.lastIoError.empty() ? "errore di ingresso/uscita" : vm.lastIoError, true), 0); }
// Memoria grezza di un puntatore: byte di un array di numeri.
struct Raw { ArrayObj* a{nullptr}; InstObj* inst{nullptr}; std::uint32_t idx{0}; std::size_t bytes{0}; };
Raw rawOf(Vm& vm, const Value& p, const char* what) {
    Raw r;
    Value x = p;
    if (x.isNull()) vm.fault(FaultKind::NullPointer, std::string(what) + ": puntatore NULL");
    if (x.k == VK::Ptr && x.f == 2) {
        vm.fault(FaultKind::NotSupported, std::string(what) + " su una variabile semplice: usalo su array o struct");
    }
    if (x.k == VK::Ref) x = Value::ptr(x.o, x.o->kind == OK::Array ? 0 : Whole);
    Obj* o = x.o;
    if (o->dead()) vm.fault(FaultKind::DeadPointer, std::string(what) + ": memoria gia' liberata");
    if (o->kind == OK::Inst && x.idx == Whole) { r.inst = static_cast<InstObj*>(o); return r; }
    if (o->kind != OK::Array) vm.fault(FaultKind::NotSupported, std::string(what) + ": serve un array o una struct");
    ArrayObj* a = static_cast<ArrayObj*>(o);
    if (a->ek == VK::Void) {
        // blocco di malloc senza tipo: byte
        vm.castTo(Value::ptr(a, 0), vm.program().charPtr);
    }
    r.a = a;
    r.idx = x.idx == Whole ? 0 : x.idx;
    r.bytes = a->refs() ? 0 : static_cast<std::size_t>(a->count - std::min(a->count, r.idx)) * a->esize;
    return r;
}
void cMemset(Vm& vm, NativeCall& c) {
    Raw r = rawOf(vm, c.args[0], "memset");
    const int v = static_cast<int>(I(c, 1));
    const std::int64_t n = I(c, 2);
    if (n < 0) vm.fault(FaultKind::Argument, "memset: dimensione negativa");
    c.ret = c.args[0];
    if (r.inst) {
        if (v != 0) vm.fault(FaultKind::NotSupported, "memset di una struct con un valore diverso da 0");
        const std::vector<TypeP>& st = vm.slotsOf(r.inst->cls);
        for (std::uint32_t i = 0; i < r.inst->nfields; ++i) r.inst->fields[i] = vm.defaultValue(i < st.size() ? st[i] : nullptr);
        return;
    }
    if (r.a->refs()) {
        if (v != 0) vm.fault(FaultKind::NotSupported, "memset con un valore diverso da 0 su un array di struct o puntatori");
        const std::size_t cnt = std::min<std::size_t>(r.a->count - r.idx, static_cast<std::size_t>(n) / std::max<std::int64_t>(1, vm.sizeOfType(r.a->elem)));
        for (std::size_t i = 0; i < cnt; ++i) r.a->set(r.idx + static_cast<std::uint32_t>(i), vm.defaultValue(r.a->elem));
        return;
    }
    if (r.a->flags & FReadOnly) vm.fault(FaultKind::ReadOnly, "memset su una stringa letterale");
    if (static_cast<std::size_t>(n) > r.bytes) vm.fault(FaultKind::Index, "memset: " + std::to_string(n) + " byte oltre la fine dell'array (" + std::to_string(r.bytes) + ")");
    std::memset(r.a->data + static_cast<std::size_t>(r.idx) * r.a->esize, v, static_cast<std::size_t>(n));
}
void memMove(Vm& vm, NativeCall& c, const char* what) {
    Raw d = rawOf(vm, c.args[0], what);
    Raw s = rawOf(vm, c.args[1], what);
    const std::int64_t n = I(c, 2);
    c.ret = c.args[0];
    if (n < 0) vm.fault(FaultKind::Argument, std::string(what) + ": dimensione negativa");
    if (d.inst || s.inst) {
        if (!(d.inst && s.inst)) vm.fault(FaultKind::NotSupported, std::string(what) + " tra una struct e un array");
        vm.copyInto(Value::ref(d.inst), Value::ref(s.inst));
        return;
    }
    if (d.a->refs() || s.a->refs()) {
        if (d.a->elem != s.a->elem && vkOf(d.a->elem) != vkOf(s.a->elem)) vm.fault(FaultKind::NotSupported, std::string(what) + " tra array di tipi diversi");
        const std::int64_t es = std::max<std::int64_t>(1, vm.sizeOfType(d.a->elem));
        const std::size_t cnt = static_cast<std::size_t>(n / es);
        if (d.idx + cnt > d.a->count || s.idx + cnt > s.a->count) vm.fault(FaultKind::Index, std::string(what) + ": oltre la fine dell'array");
        std::vector<Value> tmp;
        for (std::size_t i = 0; i < cnt; ++i) tmp.push_back(vm.copyValue(s.a->get(s.idx + static_cast<std::uint32_t>(i))));
        for (std::size_t i = 0; i < cnt; ++i) d.a->set(d.idx + static_cast<std::uint32_t>(i), tmp[i]);
        return;
    }
    if (d.a->flags & FReadOnly) vm.fault(FaultKind::ReadOnly, std::string(what) + ": scrittura su una stringa letterale");
    if (static_cast<std::size_t>(n) > d.bytes || static_cast<std::size_t>(n) > s.bytes) vm.fault(FaultKind::Index, std::string(what) + ": " + std::to_string(n) + " byte oltre la fine dell'array");
    std::memmove(d.a->data + static_cast<std::size_t>(d.idx) * d.a->esize, s.a->data + static_cast<std::size_t>(s.idx) * s.a->esize, static_cast<std::size_t>(n));
}
void cMemcpy(Vm& vm, NativeCall& c) { memMove(vm, c, "memcpy"); }
void cMemmove(Vm& vm, NativeCall& c) { memMove(vm, c, "memmove"); }
void cMemcmp(Vm& vm, NativeCall& c) {
    Raw a = rawOf(vm, c.args[0], "memcmp");
    Raw b = rawOf(vm, c.args[1], "memcmp");
    const std::int64_t n = I(c, 2);
    if (a.inst || b.inst || a.a->refs() || b.a->refs()) vm.fault(FaultKind::NotSupported, "memcmp vuole array di numeri o di char");
    if (static_cast<std::size_t>(n) > a.bytes || static_cast<std::size_t>(n) > b.bytes) vm.fault(FaultKind::Index, "memcmp: oltre la fine dell'array");
    c.ret = Value::i32(sgn(std::memcmp(a.a->data + static_cast<std::size_t>(a.idx) * a.a->esize, b.a->data + static_cast<std::size_t>(b.idx) * b.a->esize, static_cast<std::size_t>(n))));
}

// ================================================================ ctype
int kIsalpha(int x) { return std::isalpha(x); }
int kIsdigit(int x) { return std::isdigit(x); }
int kIsalnum(int x) { return std::isalnum(x); }
int kIsspace(int x) { return std::isspace(x); }
int kIsupper(int x) { return std::isupper(x); }
int kIslower(int x) { return std::islower(x); }
int kIspunct(int x) { return std::ispunct(x); }
int kIsxdigit(int x) { return std::isxdigit(x); }
int kIsprint(int x) { return std::isprint(x); }
int kIscntrl(int x) { return std::iscntrl(x); }
int kIsgraph(int x) { return std::isgraph(x); }
int kIsblank(int x) { return x == ' ' || x == '\t'; }
int kToupper(int x) { return std::toupper(x); }
int kTolower(int x) { return std::tolower(x); }
template <int (*F)(int)> void cCtype(Vm&, NativeCall& c) {
    const std::int64_t v = I(c, 0);
    const bool r = v >= 0 && v <= 255 && F(static_cast<int>(v)) != 0;
    c.ret = Value::i32(r ? 1 : 0);
}
template <int (*F)(int)> void cCconv(Vm&, NativeCall& c) {
    const std::int64_t v = I(c, 0);
    c.ret = Value::i32(v >= 0 && v <= 255 ? F(static_cast<int>(v)) : v);
}

// ================================================================ math
template <double (*F)(double)> void cMath1(Vm&, NativeCall& c) { c.ret = Value::f64(F(D(c, 0))); }
template <double (*F)(double)> void cMath1f(Vm&, NativeCall& c) { c.ret = Value::f32(F(static_cast<double>(static_cast<float>(D(c, 0))))); }
template <double (*F)(double, double)> void cMath2(Vm&, NativeCall& c) { c.ret = Value::f64(F(D(c, 0), D(c, 1))); }
template <double (*F)(double, double)> void cMath2f(Vm&, NativeCall& c) { c.ret = Value::f32(F(D(c, 0), D(c, 1))); }
double dSin(double x) { return std::sin(x); }
double dCos(double x) { return std::cos(x); }
double dTan(double x) { return std::tan(x); }
double dAsin(double x) { return std::asin(x); }
double dAcos(double x) { return std::acos(x); }
double dAtan(double x) { return std::atan(x); }
double dSinh(double x) { return std::sinh(x); }
double dCosh(double x) { return std::cosh(x); }
double dTanh(double x) { return std::tanh(x); }
double dAsinh(double x) { return std::asinh(x); }
double dAcosh(double x) { return std::acosh(x); }
double dAtanh(double x) { return std::atanh(x); }
double dExp(double x) { return std::exp(x); }
double dExp2(double x) { return std::exp2(x); }
double dExpm1(double x) { return std::expm1(x); }
double dLog(double x) { return std::log(x); }
double dLog10(double x) { return std::log10(x); }
double dLog2(double x) { return std::log2(x); }
double dLog1p(double x) { return std::log1p(x); }
double dSqrt(double x) { return std::sqrt(x); }
double dCbrt(double x) { return std::cbrt(x); }
double dCeil(double x) { return std::ceil(x); }
double dFloor(double x) { return std::floor(x); }
double dFabs(double x) { return std::fabs(x); }
double dRound(double x) { return std::round(x); }
double dTrunc(double x) { return std::trunc(x); }
double dRint(double x) { return std::nearbyint(x); }
double dErf(double x) { return std::erf(x); }
double dErfc(double x) { return std::erfc(x); }
double dTgamma(double x) { return std::tgamma(x); }
double dLgamma(double x) { return std::lgamma(x); }
double dPow(double x, double y) { return std::pow(x, y); }
double dAtan2(double x, double y) { return std::atan2(x, y); }
double dFmod(double x, double y) { return std::fmod(x, y); }
double dHypot(double x, double y) { return std::hypot(x, y); }
double dFmin(double x, double y) { return std::fmin(x, y); }
double dFmax(double x, double y) { return std::fmax(x, y); }
double dCopysign(double x, double y) { return std::copysign(x, y); }
double dRemainder(double x, double y) { return std::remainder(x, y); }
double dFdim(double x, double y) { return std::fdim(x, y); }
void cLround(Vm&, NativeCall& c) { c.ret = Value::i64(static_cast<std::int64_t>(std::llround(D(c, 0)))); }
template <int W> void cClassify(Vm& vm, NativeCall& c) {
    const double d = D(c, 0);
    const bool r = W == 0 ? std::isnan(d) : W == 1 ? std::isinf(d) : W == 2 ? std::isfinite(d) : std::signbit(d);
    (void)vm;
    c.ret = Value::i32(r ? 1 : 0);
}
void cLdexp(Vm&, NativeCall& c) { c.ret = Value::f64(std::ldexp(D(c, 0), static_cast<int>(I(c, 1)))); }
void cFrexp(Vm& vm, NativeCall& c) { int e = 0; const double m = std::frexp(D(c, 0), &e); vm.storePtr(c.args[1], Value::i32(e)); c.ret = Value::f64(m); }
void cModf(Vm& vm, NativeCall& c) { double ip = 0; const double f = std::modf(D(c, 0), &ip); vm.storePtr(c.args[1], Value::f64(ip)); c.ret = Value::f64(f); }
void cFma(Vm&, NativeCall& c) { c.ret = Value::f64(std::fma(D(c, 0), D(c, 1), D(c, 2))); }

// ================================================================ time, assert
void cTime(Vm& vm, NativeCall& c) {
    const std::int64_t t = static_cast<std::int64_t>(vm.host().millis() / 1000u);
    if (!c.args[0].isNull() && c.args[0].k == VK::Ptr) vm.storePtr(c.args[0], Value::i64(t));
    c.ret = Value::i64(t);
}
void cClock(Vm& vm, NativeCall& c) { c.ret = Value::i64(static_cast<std::int64_t>(vm.host().millis()) * 1000); }
void cDifftime(Vm&, NativeCall& c) { c.ret = Value::f64(static_cast<double>(I(c, 0) - I(c, 1))); }
void cAssert(Vm& vm, NativeCall& c) { if (!vm.truthy(c.args[0])) vm.fatal("Argument ERROR: assert fallita"); }

constexpr std::uint8_t St = NF::Static;
#define C1(scope, name, sig, fn) {L::CC, scope, "", name, sig, fn, St}
#define MATH(name, fn) C1("math.h|cmath", name, "double(double)", cMath1<fn>), C1("math.h|cmath", name "f", "float(float)", cMath1f<fn>)
#define MATH2(name, fn) C1("math.h|cmath", name, "double(double,double)", cMath2<fn>), C1("math.h|cmath", name "f", "float(float,float)", cMath2f<fn>)
const NativeEntry kC[] = {
    // stdio
    C1("stdio.h|cstdio", "printf", "int(cstr,...)", cPrintf),
    C1("stdio.h|cstdio", "fprintf", "int(file,cstr,...)", cFprintf),
    C1("stdio.h|cstdio", "sprintf", "int(charptr,cstr,...)", cSprintf),
    C1("stdio.h|cstdio", "snprintf", "int(charptr,size_t,cstr,...)", cSnprintf),
    C1("stdio.h|cstdio", "puts", "int(cstr)", cPuts),
    C1("stdio.h|cstdio", "putchar", "int(int)", cPutchar),
    C1("stdio.h|cstdio", "getchar", "int()", cGetchar),
    C1("stdio.h|cstdio", "gets", "charptr(charptr)", cGets),
    C1("stdio.h|cstdio", "scanf", "int(cstr,...)", cScanf),
    C1("stdio.h|cstdio", "sscanf", "int(cstr,cstr,...)", cSscanf),
    C1("stdio.h|cstdio", "fscanf", "int(file,cstr,...)", cFscanf),
    C1("stdio.h|cstdio", "fopen", "file(cstr,cstr)", cFopen),
    C1("stdio.h|cstdio", "fclose", "int(file)", cFclose),
    C1("stdio.h|cstdio", "fgets", "charptr(charptr,int,file)", cFgets),
    C1("stdio.h|cstdio", "fputs", "int(cstr,file)", cFputs),
    C1("stdio.h|cstdio", "fputc", "int(int,file)", cFputc),
    C1("stdio.h|cstdio", "putc", "int(int,file)", cFputc),
    C1("stdio.h|cstdio", "fgetc", "int(file)", cFgetc),
    C1("stdio.h|cstdio", "getc", "int(file)", cFgetc),
    C1("stdio.h|cstdio", "ungetc", "int(int,file)", cUngetc),
    C1("stdio.h|cstdio", "feof", "int(file)", cFeof),
    C1("stdio.h|cstdio", "ferror", "int(file)", cFerror),
    C1("stdio.h|cstdio", "clearerr", "void(file)", cClearerr),
    C1("stdio.h|cstdio", "fflush", "int(file)", cFflush),
    C1("stdio.h|cstdio", "rewind", "void(file)", cRewind),
    C1("stdio.h|cstdio", "ftell", "long(file)", cFtell),
    C1("stdio.h|cstdio", "fseek", "int(file,long,int)", cFseek),
    C1("stdio.h|cstdio", "remove", "int(cstr)", cRemove),
    C1("stdio.h|cstdio", "rename", "int(cstr,cstr)", cRename),
    C1("stdio.h|cstdio", "perror", "void(cstr)", cPerror),
    // stdlib
    C1("stdlib.h|cstdlib", "abs", "int(int)", cAbs),
    C1("stdlib.h|cstdlib", "labs", "long(long)", cLabs),
    C1("stdlib.h|cstdlib", "llabs", "long(long)", cLabs),
    C1("stdlib.h|cstdlib", "atoi", "int(cstr)", cAtoi),
    C1("stdlib.h|cstdlib", "atol", "long(cstr)", cAtol),
    C1("stdlib.h|cstdlib", "atoll", "long(cstr)", cAtol),
    C1("stdlib.h|cstdlib", "atof", "double(cstr)", cAtof),
    C1("stdlib.h|cstdlib", "strtol", "long(cstr,any,int)", cStrtol<VK::I64>),
    C1("stdlib.h|cstdlib", "strtoll", "long(cstr,any,int)", cStrtol<VK::I64>),
    C1("stdlib.h|cstdlib", "strtoul", "ulong(cstr,any,int)", cStrtol<VK::U64>),
    C1("stdlib.h|cstdlib", "strtoull", "ulong(cstr,any,int)", cStrtol<VK::U64>),
    C1("stdlib.h|cstdlib", "strtod", "double(cstr,any)", cStrtod),
    C1("stdlib.h|cstdlib", "strtof", "float(cstr,any)", cStrtof),
    C1("stdlib.h|cstdlib", "rand", "int()", cRand),
    C1("stdlib.h|cstdlib", "srand", "void(uint)", cSrand),
    C1("stdlib.h|cstdlib", "exit", "void(int)", cExit),
    C1("stdlib.h|cstdlib", "abort", "void()", cAbort),
    C1("stdlib.h|cstdlib", "system", "int(cstr)", cSystem),
    C1("stdlib.h|cstdlib", "getenv", "charptr(cstr)", cGetenv),
    C1("stdlib.h|cstdlib", "malloc", "voidptr(size_t)", cMalloc),
    C1("stdlib.h|cstdlib", "calloc", "voidptr(size_t,size_t)", cCalloc),
    C1("stdlib.h|cstdlib", "realloc", "voidptr(voidptr,size_t)", cRealloc),
    C1("stdlib.h|cstdlib", "free", "void(voidptr)", cFree),
    C1("stdlib.h|cstdlib", "qsort", "void(voidptr,size_t,size_t,func)", cQsort),
    C1("stdlib.h|cstdlib", "bsearch", "voidptr(voidptr,voidptr,size_t,size_t,func)", cBsearch),
    // string.h
    C1("string.h|cstring", "strlen", "size_t(cstr)", cStrlen),
    C1("string.h|cstring", "strcpy", "charptr(charptr,cstr)", cStrcpy),
    C1("string.h|cstring", "strncpy", "charptr(charptr,cstr,size_t)", cStrncpy),
    C1("string.h|cstring", "strcat", "charptr(charptr,cstr)", cStrcat),
    C1("string.h|cstring", "strncat", "charptr(charptr,cstr,size_t)", cStrncat),
    C1("string.h|cstring", "strcmp", "int(cstr,cstr)", cStrcmp),
    C1("string.h|cstring", "strncmp", "int(cstr,cstr,size_t)", cStrncmp),
    C1("string.h|cstring", "strchr", "charptr(cstr,int)", cStrchr),
    C1("string.h|cstring", "strrchr", "charptr(cstr,int)", cStrrchr),
    C1("string.h|cstring", "strstr", "charptr(cstr,cstr)", cStrstr),
    C1("string.h|cstring", "strspn", "size_t(cstr,cstr)", cStrspn),
    C1("string.h|cstring", "strcspn", "size_t(cstr,cstr)", cStrcspn),
    C1("string.h|cstring", "strpbrk", "charptr(cstr,cstr)", cStrpbrk),
    C1("string.h|cstring", "strtok", "charptr(charptr,cstr)", cStrtok),
    C1("string.h|cstring", "strdup", "charptr(cstr)", cStrdup),
    C1("string.h|cstring", "strerror", "charptr(int)", cStrerror),
    C1("string.h|cstring", "memset", "voidptr(voidptr,int,size_t)", cMemset),
    C1("string.h|cstring", "memcpy", "voidptr(voidptr,voidptr,size_t)", cMemcpy),
    C1("string.h|cstring", "memmove", "voidptr(voidptr,voidptr,size_t)", cMemmove),
    C1("string.h|cstring", "memcmp", "int(voidptr,voidptr,size_t)", cMemcmp),
    // ctype
    C1("ctype.h|cctype", "isalpha", "int(int)", cCtype<kIsalpha>),
    C1("ctype.h|cctype", "isdigit", "int(int)", cCtype<kIsdigit>),
    C1("ctype.h|cctype", "isalnum", "int(int)", cCtype<kIsalnum>),
    C1("ctype.h|cctype", "isspace", "int(int)", cCtype<kIsspace>),
    C1("ctype.h|cctype", "isupper", "int(int)", cCtype<kIsupper>),
    C1("ctype.h|cctype", "islower", "int(int)", cCtype<kIslower>),
    C1("ctype.h|cctype", "ispunct", "int(int)", cCtype<kIspunct>),
    C1("ctype.h|cctype", "isxdigit", "int(int)", cCtype<kIsxdigit>),
    C1("ctype.h|cctype", "isprint", "int(int)", cCtype<kIsprint>),
    C1("ctype.h|cctype", "iscntrl", "int(int)", cCtype<kIscntrl>),
    C1("ctype.h|cctype", "isgraph", "int(int)", cCtype<kIsgraph>),
    C1("ctype.h|cctype", "isblank", "int(int)", cCtype<kIsblank>),
    C1("ctype.h|cctype", "toupper", "int(int)", cCconv<kToupper>),
    C1("ctype.h|cctype", "tolower", "int(int)", cCconv<kTolower>),
    // math
    MATH("sin", dSin), MATH("cos", dCos), MATH("tan", dTan), MATH("asin", dAsin), MATH("acos", dAcos), MATH("atan", dAtan),
    MATH("sinh", dSinh), MATH("cosh", dCosh), MATH("tanh", dTanh), MATH("asinh", dAsinh), MATH("acosh", dAcosh), MATH("atanh", dAtanh),
    MATH("exp", dExp), MATH("exp2", dExp2), MATH("expm1", dExpm1), MATH("log", dLog), MATH("log10", dLog10), MATH("log2", dLog2), MATH("log1p", dLog1p),
    MATH("sqrt", dSqrt), MATH("cbrt", dCbrt), MATH("ceil", dCeil), MATH("floor", dFloor), MATH("fabs", dFabs), MATH("round", dRound),
    MATH("trunc", dTrunc), MATH("rint", dRint), MATH("nearbyint", dRint), MATH("erf", dErf), MATH("erfc", dErfc), MATH("tgamma", dTgamma), MATH("lgamma", dLgamma),
    MATH2("pow", dPow), MATH2("atan2", dAtan2), MATH2("fmod", dFmod), MATH2("hypot", dHypot), MATH2("fmin", dFmin), MATH2("fmax", dFmax),
    MATH2("copysign", dCopysign), MATH2("remainder", dRemainder), MATH2("fdim", dFdim),
    C1("math.h|cmath", "lround", "long(double)", cLround),
    C1("math.h|cmath", "llround", "long(double)", cLround),
    C1("math.h|cmath", "isnan", "int(double)", cClassify<0>),
    C1("math.h|cmath", "isinf", "int(double)", cClassify<1>),
    C1("math.h|cmath", "isfinite", "int(double)", cClassify<2>),
    C1("math.h|cmath", "signbit", "int(double)", cClassify<3>),
    C1("math.h|cmath", "ldexp", "double(double,int)", cLdexp),
    C1("math.h|cmath", "frexp", "double(double,any)", cFrexp),
    C1("math.h|cmath", "modf", "double(double,any)", cModf),
    C1("math.h|cmath", "fma", "double(double,double,double)", cFma),
    // time, assert
    C1("time.h|ctime", "time", "long(any)", cTime),
    C1("time.h|ctime", "clock", "long()", cClock),
    C1("time.h|ctime", "difftime", "double(long,long)", cDifftime),
    C1("assert.h|cassert", "assert", "void(int)", cAssert),
};
#undef C1
#undef MATH
#undef MATH2
} // namespace

const NativeEntry* libCEntries(std::size_t& n) { n = sizeof(kC) / sizeof(kC[0]); return kC; }

} // namespace fx::prog
