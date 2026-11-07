// AI CHAT: conversation with Claude or ChatGPT through the HTTPS relay.
// Provider API keys are never on the device; the relay holds them. The
// conversation is stored in the workspace and can be cleared here (F2) or in MEMORY.
#include "ai_protocol.hpp"
#include "apps.hpp"
#include "net.hpp"
#include <algorithm>

namespace fx::app {
using namespace ui::theme;

namespace {
const char* providerName(std::uint8_t p) { return p == 1 ? "ChatGPT" : "Claude"; }
constexpr std::size_t WrapSmall = 51;          // glyphs per line in the Small face (6 px)
}

void AiChatApp::enter(Context& ctx) {
    scroll_ = 0;
    net::ChatTransport* t = ctx.platform.chat();
    const Settings& s = ctx.ws.settings;
    statusError_ = false;
    if (!t) { status_ = "Rete non disponibile su questa piattaforma"; statusError_ = true; return; }
    if (s.wifiSsid.empty() || s.relayHost.empty() || s.relayToken.empty()) {
        status_ = "Configura Wi-Fi e relay in SYSTEM (F10)"; statusError_ = true; return;
    }
    // Join the network in the background while the user types.
    if (!t->wifiConnected()) t->configureWifi(s.wifiSsid, s.wifiPassword);
    status_ = std::string("Wi-Fi: ") + t->wifiStatus();
}

void AiChatApp::leave(Context& ctx) {
    if (sending_) { if (auto* t = ctx.platform.chat()) t->cancel(); sending_ = false; }
}

void AiChatApp::send(Context& ctx) {
    Settings& s = ctx.ws.settings;
    net::ChatTransport* t = ctx.platform.chat();
    std::string text = input_.text();
    while (!text.empty() && text.back() == ' ') text.pop_back();
    if (text.empty() || sending_) return;
    if (!t) throw MathError("ERROR: rete non disponibile su questa piattaforma");
    if (s.wifiSsid.empty() || s.relayHost.empty() || s.relayToken.empty())
        throw MathError("ERROR: configura Wi-Fi, relay host e token in SYSTEM");
    auto& chat = ctx.ws.chat;
    chat.push_back({true, text});
    while (chat.size() > Workspace::MaxChatMessages) chat.erase(chat.begin());
    if (!chat.empty() && !chat.front().fromUser) chat.erase(chat.begin());
    net::ChatRequest req;
    req.host = s.relayHost; req.port = s.relayPort; req.path = s.relayPath; req.token = s.relayToken;
    req.body = ai::buildChatBody(chat, s.aiProvider, s.aiModel, s.aiMaxChars);
    if (!t->wifiConnected()) t->configureWifi(s.wifiSsid, s.wifiPassword);
    if (!t->begin(req)) { chat.pop_back(); throw MathError("ERROR: richiesta gia' in corso"); }
    input_.clear();
    sending_ = true; statusError_ = false; scroll_ = 0;
    status_ = std::string("Invio a ") + providerName(s.aiProvider) + "...";
    ctx.markDirty();
}

bool AiChatApp::tick(Context& ctx) {
    net::ChatTransport* t = ctx.platform.chat();
    if (!t) return false;
    const net::ChatState st = t->poll();
    const std::uint32_t now = ctx.now();
    if (sending_) {
        switch (st) {
        case net::ChatState::Connecting: status_ = std::string("Wi-Fi: ") + t->wifiStatus() + " - connessione al relay..."; break;
        case net::ChatState::Sending: status_ = "Invio richiesta..."; break;
        case net::ChatState::Waiting: status_ = std::string("In attesa di ") + providerName(ctx.ws.settings.aiProvider) + "..."; break;
        case net::ChatState::Done: {
            sending_ = false;
            const int code = t->httpStatus();
            const std::string body = ai::sanitizeText(t->response());
            if (code == 200) {
                ctx.ws.chat.push_back({false, body.empty() ? std::string("(risposta vuota)") : body});
                status_ = std::string("Risposta di ") + providerName(ctx.ws.settings.aiProvider);
                statusError_ = false;
                ctx.markDirty();
            } else {
                status_ = "Errore " + std::to_string(code) + ": " + (body.empty() ? std::string("relay") : body);
                statusError_ = true;
            }
            t->cancel();
            return true;
        }
        case net::ChatState::Failed:
            sending_ = false;
            status_ = "Errore: " + t->error();
            statusError_ = true;
            t->cancel();
            return true;
        case net::ChatState::Idle: break;
        }
        // Animated status while waiting, at most 4 redraws per second.
        if (now - lastDraw_ >= 250) { lastDraw_ = now; return true; }
        return false;
    }
    // Idle: keep the Wi-Fi status line fresh.
    if (!statusError_ && now - lastDraw_ >= 1000) {
        const std::string s = std::string("Wi-Fi: ") + t->wifiStatus();
        lastDraw_ = now;
        if (s != status_ && status_.rfind("Wi-Fi:", 0) == 0) { status_ = s; return true; }
    }
    return false;
}

void AiChatApp::key(Context& ctx, const KeyEvent& ev) {
    Settings& s = ctx.ws.settings;
    switch (ev.key) {
    case Key::Exe: case Key::F1: send(ctx); return;
    case Key::F2: ctx.ws.chat.clear(); scroll_ = 0; status_ = "Nuova conversazione"; statusError_ = false; ctx.markDirty(); return;
    case Key::F3: if (!sending_) { s.aiProvider = s.aiProvider ? 0 : 1; status_ = std::string("Provider: ") + providerName(s.aiProvider); statusError_ = false; ctx.markDirty(); } return;
    case Key::F4:
        if (auto* t = ctx.platform.chat()) {
            if (s.wifiSsid.empty()) throw MathError("ERROR: configura la rete Wi-Fi in SYSTEM");
            t->configureWifi(s.wifiSsid, s.wifiPassword);
            status_ = std::string("Wi-Fi: ") + t->wifiStatus(); statusError_ = false;
        }
        return;
    case Key::F5: if (sending_) { if (auto* t = ctx.platform.chat()) t->cancel(); sending_ = false; status_ = "Richiesta annullata"; statusError_ = true; } return;
    case Key::F6: ctx.open(AppId::System); return;
    case Key::Up: ++scroll_; return;
    case Key::Down: scroll_ = std::max(0, scroll_ - 1); return;
    case Key::PageUp: scroll_ += 10; return;
    case Key::PageDown: scroll_ = std::max(0, scroll_ - 10); return;
    case Key::Exit:
        if (sending_) { key(ctx, {Key::F5}); return; }
        ctx.home(); return;
    default:
        if (input_.key(ev)) scroll_ = 0;
        return;
    }
}

void AiChatApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ui::StatusInfo st = ctx.status("AI CHAT");
    st.busy = sending_;
    if (auto* t = ctx.platform.chat()) st.wifi = t->wifiConnected();
    ui::statusBar(c, st);
    const Settings& s = ctx.ws.settings;
    const int top = ui::ContentTop, inputTop = ui::ContentBottom - 22, statusTop = inputTop - 15;
    // Header: provider and model.
    c.rect(0, top, ui::ScreenW, 16, SelectSoft, true);
    std::string head = std::string(providerName(s.aiProvider)) + (s.aiModel.empty() ? "  (modello predefinito dal relay)" : "  " + s.aiModel);
    ui::textFit(c, 4, top + 2, head, ui::ScreenW - 8, Ink, font::Face::Small);
    // Conversation, wrapped and laid out bottom-up.
    struct Line { std::string text; bool user; bool first; };
    std::vector<Line> lines;
    std::vector<std::string> wrapped(200);
    for (const auto& m : ctx.ws.chat) {
        const std::size_t n = ui::wrapText((m.fromUser ? "Tu: " : "") + m.text, WrapSmall, wrapped.data(), wrapped.size());
        for (std::size_t i = 0; i < n; ++i) lines.push_back({wrapped[i], m.fromUser, i == 0});
        lines.push_back({"", m.fromUser, false});
    }
    const int lineH = 13, areaTop = top + 18, areaBottom = statusTop - 2;
    const int visible = (areaBottom - areaTop) / lineH;
    const int total = static_cast<int>(lines.size());
    scroll_ = std::clamp(scroll_, 0, std::max(0, total - visible));
    const int first = std::max(0, total - visible - scroll_);
    for (int i = 0; i < visible && first + i < total; ++i) {
        const Line& l = lines[static_cast<std::size_t>(first + i)];
        const int y = areaTop + i * lineH;
        if (l.user && !l.text.empty()) c.rect(0, y - 1, ui::ScreenW - 5, lineH, SelectSoft, true);
        ui::text(c, 4, y, l.text.c_str(), l.user ? Accent : Ink, font::Face::Small);
    }
    if (lines.empty()) {
        ui::textCenter(c, ui::ScreenW / 2, areaTop + 40, "Scrivi una domanda e premi EXE", Muted);
        ui::textCenter(c, ui::ScreenW / 2, areaTop + 62, "Le chiavi API restano sul relay HTTPS", Muted, font::Face::Small);
    }
    ui::scrollBar(c, ui::ScreenW - 4, areaTop, visible * lineH, total, visible, first);
    // Status line and input.
    c.hLine(0, statusTop - 1, ui::ScreenW, Faint);
    static const char* spin[] = {"|", "/", "-", "\\"};
    std::string statusText = sending_ ? std::string(spin[(ctx.now() / 250) % 4]) + " " + status_ : status_;
    ui::textFit(c, 4, statusTop + 1, statusText, ui::ScreenW - 8, statusError_ ? Error : Muted, font::Face::Small);
    c.rect(0, inputTop, ui::ScreenW, 22, StatusBg, true);
    c.hLine(0, inputTop, ui::ScreenW, Faint);
    input_.draw(c, 4, inputTop + 3, ui::ScreenW - 8, Ink, !sending_);
    const char* keys[6] = {"SEND", "NEW", s.aiProvider ? "\xE2\x86\x92" "Claude" : "\xE2\x86\x92" "GPT", "WIFI", sending_ ? "STOP" : nullptr, "SETUP"};
    ui::softKeys(c, keys);
}
} // namespace fx::app
