// LINK (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Trasferimento verso un PC sulla USB seriale con il formato PROPRIO del
// progetto (link_protocol.hpp; il protocollo Link Casio tra calcolatrici non
// e' usato): invio del workspace o di un file dell'utente, ricezione di un
// pacchetto .pclink (conservato in PICOCALC/LINK e poi applicato), avanzamento
// a schermo, AC per annullare. Lo strumento per il PC e' tools/picocalc_link.py.
#include "apps.hpp"
#include "link_protocol.hpp"
#include "limits.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Act : int { ActSendAll = 0, ActSendFile, ActReceive, ActPackets, ActApply, ActDelete };
const FKeyPage kSendPage{{{"ALL", ActSendAll}, {"FILE", ActSendFile}, {}, {}, {}, {}}};
const FKeyMenu kSendMenu{&kSendPage, 1, nullptr};
const FKeyPage kMainPage{{{"SEND", -1, &kSendMenu}, {"RECEIVE", ActReceive}, {"PACKETS", ActPackets}, {}, {}, {}}};
const FKeyMenu kMainMenu{&kMainPage, 1, nullptr};
const FKeyPage kPacketPage{{{"APPLY", ActApply}, {"DELETE", ActDelete}, {}, {}, {}, {}}};
const FKeyMenu kPacketMenu{&kPacketPage, 1, nullptr};
const FKeyPage kNonePage{{{}, {}, {}, {}, {}, {}}};
const FKeyMenu kNoneMenu{&kNonePage, 1, nullptr};
enum Screen : int { Main, Transfer, Files, Packets, Contents };
constexpr int ConfirmWorkspace = 1, ConfirmDeletePacket = 2;
} // namespace

struct LinkApp::View {
    ModeShell shell;
    TypedFileMenu packets{files::FileType::Link};
    Screen screen{Main};
    std::unique_ptr<link::Sender> sender;
    std::unique_ptr<link::Receiver> receiver;
    step::Runner runner;
    std::string title;
    SelectList list;
    std::vector<std::pair<files::FileType, std::string>> userFiles;
    std::vector<files::TypedEntry> packetFiles;
    std::vector<link::Entry> contents;
    std::string contentsFile;
};

LinkApp::LinkApp() = default;
LinkApp::~LinkApp() = default;

namespace {
link::Port& port(Context& ctx) {
    link::Port* p = ctx.platform.serialPort();
    if (!p) throw MathError("ERROR: USB seriale non disponibile");
    return *p;
}
files::FileSystem& store(Context& ctx) {
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) throw MathError("ERROR: archivio dei file non disponibile (microSD assente)");
    return *fs;
}
void startSend(Context& ctx, LinkApp::View& v, std::vector<link::Entry> entries, std::string name) {
    v.runner.reset();
    v.receiver.reset();
    v.sender = std::make_unique<link::Sender>(port(ctx), [&ctx] { return ctx.now(); }, link::encodePackage(entries), name);
    if (v.sender->status() == link::Status::Failed) throw MathError(v.sender->error());
    v.runner.start(*v.sender, step::Runner::Config{64, 25});
    v.title = "SEND " + name;
    v.screen = Transfer;
    v.shell.fkeys.set(&kNoneMenu);
}
std::string packetName(const std::string& name) {
    std::string s;
    for (char c : name) if (std::isalnum(static_cast<unsigned char>(c)) && s.size() < 8) s += c;
    return s.empty() ? "RECV" : s;
}
void finish(Context& ctx, LinkApp::View& v) {
    link::State& st = ctx.ws.link;
    if (v.sender) {
        if (v.sender->status() == link::Status::Done) { ++st.sent; st.lastBytes = static_cast<std::uint32_t>(v.sender->total()); st.lastError.clear(); }
        else st.lastError = v.sender->error();
        st.lastName = v.title;
    } else if (v.receiver) {
        if (v.receiver->status() == link::Status::Done) {
            ++st.received;
            st.lastBytes = static_cast<std::uint32_t>(v.receiver->package().size());
            st.lastName = v.receiver->name();
            st.lastError.clear();
            // Il pacchetto si conserva prima di applicarlo (PACKETS > APPLY).
            std::vector<link::Entry> entries;
            if (!link::decodePackage(v.receiver->package().data(), v.receiver->package().size(), entries)) st.lastError = "Data ERROR: pacchetto non valido";
            else {
                files::TypedStore ts(store(ctx));
                const std::string file = files::TypedStore::fileName(files::FileType::Link, packetName(v.receiver->name()));
                const files::Status s = ts.save(files::FileType::Link, file, v.receiver->package());
                if (s != files::Status::Ok) st.lastError = std::string("ERROR: ") + files::statusMessage(s);
                else { v.contents = std::move(entries); v.contentsFile = file; }
            }
        } else {
            st.lastError = v.receiver->error();
        }
    }
    ctx.markDirty();
}
void listPackets(Context& ctx, LinkApp::View& v) {
    files::TypedStore ts(store(ctx));
    v.packetFiles.clear();
    (void)ts.list(files::FileType::Link, v.packetFiles);
    v.list.items.clear();
    for (const auto& e : v.packetFiles) v.list.items.push_back(e.file + "  " + std::to_string(e.size) + " B");
    v.list.selected = 0;
    v.screen = Packets;
    v.shell.fkeys.set(&kPacketMenu);
}
void openContents(Context& ctx, LinkApp::View& v, const std::string& file) {
    files::TypedStore ts(store(ctx));
    std::vector<std::uint8_t> data;
    const files::Status s = ts.open(files::FileType::Link, file, data);
    if (s != files::Status::Ok) throw MathError(std::string("Data ERROR: ") + files::statusMessage(s));
    if (!link::decodePackage(data.data(), data.size(), v.contents)) throw MathError("Data ERROR: pacchetto non valido");
    v.contentsFile = file;
}
void applyFiles(Context& ctx, LinkApp::View& v) {
    files::FileSystem& fs = store(ctx);
    std::size_t n = 0;
    for (const link::Entry& e : v.contents) {
        if (e.kind != link::Entry::Kind::File) continue;
        // La cartella della voce (PICOCALC/<TIPO>) e poi il file con la scrittura atomica.
        const std::size_t slash = e.path.rfind('/');
        if (slash != std::string::npos) (void)fs.makeDirs(e.path.substr(0, slash));
        const files::Status s = files::writeAtomic(fs, e.path, e.data.data(), e.data.size());
        if (s != files::Status::Ok) throw MathError(std::string("ERROR: ") + e.path + ": " + files::statusMessage(s));
        ++n;
    }
    ctx.toast(std::to_string(n) + " file scritti");
}
} // namespace

void LinkApp::enter(Context&) {
    v_ = std::make_unique<View>();
    v_->shell.fkeys.set(&kMainMenu);
}
void LinkApp::leave(Context&) {
    if (v_ && v_->runner.running()) v_->runner.interrupt();      // uscendo durante un trasferimento: ABORT al PC
    v_.reset();
}
std::string LinkApp::screenName() const {
    if (!v_) return "closed";
    static const char* const n[] = {"main", "transfer", "files", "packets", "contents"};
    return n[v_->screen];
}
bool LinkApp::tick(Context& ctx) {
    if (!v_ || (!v_->sender && !v_->receiver)) return false;
    if (v_->runner.running()) v_->runner.tick([&ctx] { return ctx.now(); });
    if (!v_->runner.running()) {
        finish(ctx, *v_);
        const bool received = v_->receiver && v_->receiver->status() == link::Status::Done && !v_->contents.empty();
        const std::string err = ctx.ws.link.lastError;
        v_->sender.reset();
        v_->receiver.reset();
        if (received) { v_->screen = Contents; v_->shell.fkeys.set(&kPacketMenu); }
        else { v_->screen = Main; v_->shell.fkeys.set(&kMainMenu); }
        if (!err.empty()) throw MathError(err.find("ERROR") != std::string::npos || err.rfind("Break", 0) == 0 || err.rfind("Time Out", 0) == 0 ? err : "ERROR: " + err);
        ctx.toast(received ? "Pacchetto ricevuto: " + v_->contentsFile : "Trasferimento completato");
    }
    return true;
}

void LinkApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    const bool modal = v.shell.modalKey(ev,
        [&](int tag, bool yes) {
            if (!yes) return;
            if (tag == ConfirmDeletePacket) {
                files::TypedStore ts(store(ctx));
                (void)ts.remove(files::FileType::Link, v.packetFiles[static_cast<std::size_t>(v.list.selected)].file);
                listPackets(ctx, v);
                return;
            }
            // Workspace del pacchetto: sostituisce tutti i dati (garanzia forte: record non valido = nulla cambia).
            for (const link::Entry& e : v.contents)
                if (e.kind == link::Entry::Kind::Workspace && !deserializeWorkspace(e.data.data(), e.data.size(), ctx.ws))
                    throw MathError("Data ERROR: workspace del pacchetto non valido");
            applyFiles(ctx, v);
            ctx.markDirty();
            ctx.ws.apply();
            v.screen = Main;
            v.shell.fkeys.set(&kMainMenu);
        },
        [](int) {}, [](int, int) {});
    if (modal) return;
    if (v.screen == Transfer) {
        if (ev.is(Key::AC) || ev.is(Key::Exit)) { v.runner.interrupt(); (void)tick(ctx); }
        return;
    }
    const int a = v.shell.fkeys.key(ev, ctx.shift());
    if (a == FKeyBar::Handled) return;
    switch (v.screen) {
    case Main:
        if (a == ActSendAll) {
            link::Entry e;
            e.kind = link::Entry::Kind::Workspace;
            e.data = serializeWorkspace(ctx.ws);
            startSend(ctx, v, {std::move(e)}, "WORKSPACE");
            return;
        }
        if (a == ActSendFile) {
            files::TypedStore ts(store(ctx));
            v.userFiles.clear();
            v.list.items.clear();
            for (std::size_t t = 0; t < files::FileTypeCount; ++t) {
                const auto type = static_cast<files::FileType>(t);
                if (!files::fileTypeAvailable(type) || type == files::FileType::Link) continue;
                std::vector<files::TypedEntry> es;
                if (ts.list(type, es) != files::Status::Ok) continue;
                for (const auto& e : es) { v.userFiles.push_back({type, e.file}); v.list.items.push_back(std::string(files::fileType(type).name) + ": " + e.file); }
            }
            if (v.userFiles.empty()) throw MathError("ERROR: nessun file da inviare");
            v.list.selected = 0;
            v.screen = Files;
            v.shell.fkeys.set(&kNoneMenu);
            return;
        }
        if (a == ActReceive) {
            (void)store(ctx);                                          // il pacchetto ricevuto va salvato
            v.runner.reset();
            v.sender.reset();
            v.contents.clear();
            v.receiver = std::make_unique<link::Receiver>(port(ctx), [&ctx] { return ctx.now(); }, limits::ram().graphLayerCache ? link::MaxPackageBytes : 256u * 1024u);
            v.runner.start(*v.receiver, step::Runner::Config{64, 25});
            v.title = "RECEIVE";
            v.screen = Transfer;
            v.shell.fkeys.set(&kNoneMenu);
            return;
        }
        if (a == ActPackets) { listPackets(ctx, v); return; }
        if (ev.is(Key::Exit)) { if (v.shell.fkeys.inSubmenu()) v.shell.fkeys.back(); else ctx.home(); }
        return;
    case Files:
        if (ev.is(Key::Exe)) {
            const auto& f = v.userFiles[static_cast<std::size_t>(v.list.selected)];
            files::TypedStore ts(store(ctx));
            std::vector<std::uint8_t> raw;
            const files::Status s = files::readAll(ts.fileSystem(), files::TypedStore::path(f.first, f.second), raw, link::MaxPackageBytes);
            if (s != files::Status::Ok) throw MathError(std::string("ERROR: ") + files::statusMessage(s));
            link::Entry e;
            e.path = files::TypedStore::path(f.first, f.second);
            e.data = std::move(raw);
            startSend(ctx, v, {std::move(e)}, f.second);
            return;
        }
        if (ev.is(Key::Exit)) { v.screen = Main; v.shell.fkeys.set(&kMainMenu); return; }
        (void)v.list.key(ev);
        return;
    case Packets:
        if (v.packetFiles.empty()) { if (ev.is(Key::Exit)) { v.screen = Main; v.shell.fkeys.set(&kMainMenu); } return; }
        if (a == ActDelete) { v.shell.confirm.open("Eliminare " + v.packetFiles[static_cast<std::size_t>(v.list.selected)].file + "?", ConfirmDeletePacket); return; }
        if (a == ActApply || ev.is(Key::Exe)) {
            openContents(ctx, v, v.packetFiles[static_cast<std::size_t>(v.list.selected)].file);
            v.screen = Contents;
            return;
        }
        if (ev.is(Key::Exit)) { v.screen = Main; v.shell.fkeys.set(&kMainMenu); return; }
        (void)v.list.key(ev);
        return;
    case Contents:
        if (a == ActApply || ev.is(Key::Exe)) {
            bool workspace = false;
            for (const auto& e : v.contents) workspace = workspace || e.kind == link::Entry::Kind::Workspace;
            if (workspace) { v.shell.confirm.open("Sostituire tutti i dati con il workspace ricevuto?", ConfirmWorkspace); return; }
            applyFiles(ctx, v);
            v.screen = Main; v.shell.fkeys.set(&kMainMenu);
            return;
        }
        if (ev.is(Key::Exit)) { v.screen = Main; v.shell.fkeys.set(&kMainMenu); }
        return;
    default: return;
    }
}

void LinkApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    ui::clear(c);
    ctx.drawStatus(c, "LINK");
    const int top = ui::ContentTop;
    const link::State& st = ctx.ws.link;
    switch (v.screen) {
    case Main: {
        drawModeHeader(c, "USB seriale: formato PicoCalc con CRC-32");
        const bool usb = ctx.platform.serialPort() != nullptr;
        ui::text(c, 8, top + 26, usb ? "Porta: USB (CDC)" : "Porta: non disponibile", usb ? Ink : Error);
        char line[96];
        std::snprintf(line, sizeof line, "Inviati: %u   Ricevuti: %u", static_cast<unsigned>(st.sent), static_cast<unsigned>(st.received));
        ui::text(c, 8, top + 48, line, Ink);
        if (!st.lastName.empty()) {
            std::snprintf(line, sizeof line, "Ultimo: %s (%u B)", st.lastName.c_str(), static_cast<unsigned>(st.lastBytes));
            ui::text(c, 8, top + 70, line, Muted, font::Face::Small);
        }
        if (!st.lastError.empty()) ui::textFit(c, 8, top + 88, st.lastError, ui::ScreenW - 16, Error, font::Face::Small);
        ui::text(c, 8, top + 120, "PC: tools/picocalc_link.py send|receive", Muted, font::Face::Small);
        break;
    }
    case Transfer: {
        drawModeHeader(c, v.title);
        const int pct = std::max(0, v.runner.percent());
        const bool waiting = v.receiver && !v.receiver->started();
        ui::textCenter(c, ui::ScreenW / 2, 110, waiting ? "In attesa del PC..." : "Trasferimento...", Ink);
        const gfx::Rect bar{30, 140, ui::ScreenW - 60, 16};
        c.rect(bar.x, bar.y, bar.w, bar.h, Faint, false);
        c.rect(bar.x + 1, bar.y + 1, (bar.w - 2) * pct / 100, bar.h - 2, Accent, true);
        ui::textCenter(c, ui::ScreenW / 2, 162, (std::to_string(pct) + "%").c_str(), Ink);
        ui::textCenter(c, ui::ScreenW / 2, 200, "AC: annulla", Muted, font::Face::Small);
        break;
    }
    case Files: case Packets:
        drawModeHeader(c, v.screen == Files ? "SEND: scegli il file (EXE)" : "Pacchetti ricevuti (PICOCALC/LINK)");
        if (v.list.items.empty()) ui::textCenter(c, ui::ScreenW / 2, 120, "Nessun pacchetto", Muted);
        else v.list.draw(c, gfx::Rect{0, top + 18, ui::ScreenW, ui::ContentBottom - top - 18}, 20);
        break;
    case Contents: {
        drawModeHeader(c, v.contentsFile + ": APPLY scrive i file");
        int y = top + 22;
        for (const link::Entry& e : v.contents) {
            const std::string t = (e.kind == link::Entry::Kind::Workspace ? std::string("Workspace") : e.path) + "  " + std::to_string(e.data.size()) + " B";
            ui::textFit(c, 6, y, t, ui::ScreenW - 12, Ink, font::Face::Small);
            y += 16;
            if (y > ui::ContentBottom - 16) break;
        }
        break;
    }
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
}
} // namespace fx::app
