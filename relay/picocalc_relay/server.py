"""PicoCalc FX AI relay HTTP(S) server.

Protocol (calculator -> relay):
  POST /v1/chat
  Authorization: Bearer <device token>
  Content-Type: application/json
  {"provider": "claude" | "chatgpt", "model": "", "messages": [{"role": "user"|"assistant", "content": "..."}]}

Success: 200, text/plain; charset=utf-8, the assistant reply (trimmed to max_reply_chars),
         headers X-Relay-Model and X-Relay-Truncated.
Errors:  4xx/5xx, text/plain; charset=utf-8, one short human-readable line.
GET /healthz -> 200 "ok".
Message contents are never logged; only metadata (device name, provider, status, latency).
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import logging
import os
import ssl
import sys
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from .config import RelayConfig, load_config
from .errors import RelayError
from .ratelimit import RateLimiter

log = logging.getLogger("picocalc_relay")

# Characters the calculator font lacks are mapped to close equivalents.
_TRANSLATE = str.maketrans({
    "‘": "'", "’": "'", "“": '"', "”": '"', "–": "-", "—": "-",
    "−": "-", " ": " ", " ": " ", " ": " ", "•": "-", "´": "'",
    "\r": "",
})


def sanitize_reply(text: str, limit: int) -> tuple[str, bool]:
    """Plain text for the device: typographic punctuation replaced, Markdown
    emphasis markers removed, length bounded."""
    text = text.translate(_TRANSLATE).replace("**", "").replace("__", "")
    lines = [line[2:] if line.startswith(("# ", "## ")) else line for line in text.split("\n")]
    text = "\n".join(lines).strip()
    if len(text) > limit:
        return text[: max(0, limit - 3)].rstrip() + "...", True
    return text, False


class Relay:
    """Request processing independent of the HTTP layer (unit-testable)."""

    def __init__(self, config: RelayConfig, providers: dict, clock=time.time):
        self.config = config
        self.providers = providers
        self.limiter = RateLimiter(config.rate_per_minute, config.rate_per_day, clock)
        self._hashes = [(d.name, bytes.fromhex(d.token_sha256)) for d in config.devices]

    def authenticate(self, header: str | None) -> str:
        if not header or not header.startswith("Bearer "):
            raise RelayError(401, "Token del dispositivo mancante")
        token = header[len("Bearer "):].strip()
        digest = hashlib.sha256(token.encode("utf-8")).digest()
        name = None
        for device, expected in self._hashes:      # constant-time comparison, no early exit
            if hmac.compare_digest(digest, expected):
                name = device
        if name is None:
            raise RelayError(401, "Token del dispositivo non valido")
        return name

    def parse(self, body: bytes) -> tuple[str, str | None, list[dict], int]:
        cfg = self.config
        try:
            data = json.loads(body.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            raise RelayError(400, "Richiesta non valida: JSON atteso")
        if not isinstance(data, dict):
            raise RelayError(400, "Richiesta non valida: oggetto JSON atteso")
        provider = data.get("provider")
        if provider not in self.providers:
            raise RelayError(400, "Provider non valido (claude o chatgpt)")
        max_chars = data.get("max_reply_chars", cfg.max_reply_chars)
        if isinstance(max_chars, bool) or not isinstance(max_chars, int):
            raise RelayError(400, "max_reply_chars non valido")
        max_chars = max(200, min(max_chars, cfg.max_reply_chars))
        model = data.get("model") or None
        if model is not None:
            if not isinstance(model, str) or model not in cfg.allowed_models.get(provider, []):
                raise RelayError(400, "Modello non consentito dal relay")
        messages = data.get("messages")
        if not isinstance(messages, list) or not messages:
            raise RelayError(400, "Nessun messaggio")
        if len(messages) > cfg.max_messages:
            messages = messages[-cfg.max_messages:]        # keep the most recent turns
            while messages and messages[0].get("role") != "user":
                messages = messages[1:]
        clean = []
        for m in messages:
            if not isinstance(m, dict) or m.get("role") not in ("user", "assistant") or not isinstance(m.get("content"), str):
                raise RelayError(400, "Messaggio non valido")
            content = m["content"].strip()
            if not content:
                raise RelayError(400, "Messaggio vuoto")
            if len(content) > cfg.max_message_chars:
                raise RelayError(413, "Messaggio troppo lungo")
            clean.append({"role": m["role"], "content": content})
        if not clean or clean[0]["role"] != "user" or clean[-1]["role"] != "user":
            raise RelayError(400, "La conversazione deve iniziare e finire con un messaggio utente")
        return provider, model, clean, max_chars

    def handle(self, auth_header: str | None, body: bytes) -> tuple[int, dict, str]:
        """Returns (status, extra headers, plain-text body)."""
        try:
            device = self.authenticate(auth_header)
            if len(body) > self.config.max_request_bytes:
                raise RelayError(413, "Richiesta troppo grande")
            provider, model, messages, max_chars = self.parse(body)
            reason = self.limiter.check(device)
            if reason:
                raise RelayError(429, reason)
            started = time.monotonic()
            reply = self.providers[provider].chat(messages, model, max_chars)
            text, cut = sanitize_reply(reply.text, max_chars)
            log.info("device=%s provider=%s model=%s status=200 latency=%.1fs", device, provider, reply.model,
                     time.monotonic() - started)
            return 200, {"X-Relay-Model": reply.model, "X-Relay-Truncated": "1" if (cut or reply.truncated) else "0"}, text
        except RelayError as e:
            log.info("status=%d reason=%s", e.status, e.message)
            return e.status, {}, e.message
        except Exception:                                    # never leak internals to the device
            log.exception("unexpected relay error")
            return 500, {}, "Errore interno del relay"


def make_handler(relay: Relay):
    class Handler(BaseHTTPRequestHandler):
        server_version = "PicoCalcRelay/1.6"
        sys_version = ""

        def log_message(self, fmt, *args):                  # metadata only, via logging
            log.debug("%s %s", self.address_string(), fmt % args)

        def _send(self, status: int, text: str, headers: dict | None = None):
            payload = text.encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Connection", "close")
            self.send_header("Cache-Control", "no-store")
            for k, v in (headers or {}).items():
                self.send_header(k, v)
            self.end_headers()
            self.wfile.write(payload)

        def do_GET(self):
            if self.path == "/healthz":
                self._send(200, "ok")
            else:
                self._send(404, "Non trovato")

        def do_POST(self):
            if self.path != "/v1/chat":
                self._send(404, "Non trovato")
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
            except ValueError:
                self._send(400, "Content-Length non valido")
                return
            if length < 0 or length > relay.config.max_request_bytes:
                self._send(413, "Richiesta troppo grande")
                return
            body = self.rfile.read(length)
            status, headers, text = relay.handle(self.headers.get("Authorization"), body)
            self._send(status, text, headers)

    return Handler


def build_providers(config: RelayConfig) -> dict:
    from .providers.anthropic_provider import AnthropicProvider
    from .providers.openai_provider import OpenAIProvider
    providers = {"claude": AnthropicProvider(config.anthropic_model, config.anthropic_effort,
                                             config.anthropic_max_tokens, config.request_timeout_s)}
    if config.openai_model:
        providers["chatgpt"] = OpenAIProvider(config.openai_model, config.openai_max_tokens, config.request_timeout_s)
    return providers


def serve(config: RelayConfig, host: str, port: int, cert: str | None, key: str | None, providers=None):
    relay = Relay(config, providers if providers is not None else build_providers(config))
    httpd = ThreadingHTTPServer((host, port), make_handler(relay))
    if cert and key:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        ctx.load_cert_chain(cert, key)
        httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)
    return httpd


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="PicoCalc FX AI relay")
    ap.add_argument("--config", default=os.environ.get("PICOCALC_RELAY_CONFIG", "relay.json"))
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=8443)
    ap.add_argument("--cert", help="TLS certificate chain (PEM). Omit only behind a TLS reverse proxy.")
    ap.add_argument("--key", help="TLS private key (PEM)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args(argv)
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    config = load_config(args.config)
    if not args.cert:
        log.warning("TLS disabilitato: usare solo dietro un reverse proxy HTTPS su rete fidata")
    httpd = serve(config, args.host, args.port, args.cert, args.key)
    log.info("relay in ascolto su %s:%d (%d dispositivi)", args.host, args.port, len(config.devices))
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
