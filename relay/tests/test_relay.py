"""Relay tests. No real API keys are used and nothing is billed: provider
adapters run through the official SDKs against local fake API servers."""
import hashlib
import json
import os
import ssl
import subprocess
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import anthropic
import openai

from picocalc_relay.config import Device, RelayConfig, load_config
from picocalc_relay.errors import RelayError
from picocalc_relay.providers import Reply
from picocalc_relay.providers.anthropic_provider import AnthropicProvider
from picocalc_relay.providers.openai_provider import OpenAIProvider
from picocalc_relay.ratelimit import RateLimiter
from picocalc_relay.server import Relay, sanitize_reply, serve

TOKEN = "test-device-token-0123456789"
HASH = hashlib.sha256(TOKEN.encode()).hexdigest()


class FakeProvider:
    def __init__(self, text="Risposta", error=None):
        self.text, self.error, self.calls = text, error, []

    def chat(self, messages, model=None, max_chars=None):
        self.calls.append((messages, model))
        self.max_chars = max_chars
        if self.error:
            raise self.error
        return Reply(self.text, model or "fake-model")


def config(**kw):
    c = RelayConfig(devices=[Device("calc", HASH)], **kw)
    c.validate()
    return c


def body(provider="claude", messages=None, **extra):
    d = {"provider": provider, "messages": messages or [{"role": "user", "content": "Quanto fa 2+2?"}]}
    d.update(extra)
    return json.dumps(d).encode()


class RelayLogicTest(unittest.TestCase):
    def test_success_and_auth(self):
        p = FakeProvider("4")
        r = Relay(config(), {"claude": p})
        status, headers, text = r.handle("Bearer " + TOKEN, body())
        self.assertEqual((status, text), (200, "4"))
        self.assertEqual(headers["X-Relay-Model"], "fake-model")
        self.assertEqual(r.handle(None, body())[0], 401)
        self.assertEqual(r.handle("Bearer wrong", body())[0], 401)
        self.assertEqual(r.handle("Basic " + TOKEN, body())[0], 401)
        self.assertEqual(len(p.calls), 1)          # unauthenticated requests never reach the provider

    def test_validation(self):
        r = Relay(config(), {"claude": FakeProvider()})
        auth = "Bearer " + TOKEN
        self.assertEqual(r.handle(auth, b"not json")[0], 400)
        self.assertEqual(r.handle(auth, b"[1,2]")[0], 400)
        self.assertEqual(r.handle(auth, body(provider="gemini"))[0], 400)
        self.assertEqual(r.handle(auth, body(messages=[{"role": "assistant", "content": "x"}]))[0], 400)
        self.assertEqual(r.handle(auth, body(messages=[{"role": "system", "content": "x"}]))[0], 400)
        self.assertEqual(r.handle(auth, body(messages=[{"role": "user", "content": "   "}]))[0], 400)
        self.assertEqual(r.handle(auth, body(messages=[{"role": "user", "content": "x" * 5000}]))[0], 413)
        self.assertEqual(r.handle(auth, body(model="claude-opus-5-5"))[0], 400)    # not in allowed_models
        self.assertEqual(r.handle(auth, b"x" * 20000)[0], 413)

    def test_allowed_model_and_history_trim(self):
        p = FakeProvider()
        r = Relay(config(max_messages=3, allowed_models={"claude": ["claude-haiku-5-5"]}), {"claude": p})
        msgs = [{"role": "user", "content": "a"}, {"role": "assistant", "content": "b"}] * 3 + [{"role": "user", "content": "c"}]
        status, _, _ = r.handle("Bearer " + TOKEN, body(messages=msgs, model="claude-haiku-5-5"))
        self.assertEqual(status, 200)
        sent, model = p.calls[0]
        self.assertEqual(model, "claude-haiku-5-5")
        self.assertEqual(sent[0]["role"], "user")
        self.assertEqual(sent[-1]["content"], "c")
        self.assertLessEqual(len(sent), 3)

    def test_rate_limit(self):
        now = [1000.0]
        r = Relay(config(rate_per_minute=2, rate_per_day=3), {"claude": FakeProvider()}, clock=lambda: now[0])
        auth = "Bearer " + TOKEN
        self.assertEqual([r.handle(auth, body())[0] for _ in range(3)], [200, 200, 429])
        now[0] += 61
        self.assertEqual(r.handle(auth, body())[0], 200)
        self.assertEqual(r.handle(auth, body())[0], 429)      # daily limit (3)
        now[0] += 86400
        self.assertEqual(r.handle(auth, body())[0], 200)

    def test_provider_errors_and_internal_errors(self):
        r = Relay(config(), {"claude": FakeProvider(error=RelayError(504, "Timeout"))})
        self.assertEqual(r.handle("Bearer " + TOKEN, body())[:3:2], (504, "Timeout"))
        r = Relay(config(), {"claude": FakeProvider(error=ValueError("secret detail"))})
        status, _, text = r.handle("Bearer " + TOKEN, body())
        self.assertEqual(status, 500)
        self.assertNotIn("secret", text)

    def test_reply_length_hint(self):
        p = FakeProvider("x" * 1000)
        r = Relay(config(max_reply_chars=600), {"claude": p})
        status, headers, text = r.handle("Bearer " + TOKEN, body(max_reply_chars=300))
        self.assertEqual((status, len(text), headers["X-Relay-Truncated"]), (200, 300, "1"))
        self.assertEqual(p.max_chars, 300)
        r.handle("Bearer " + TOKEN, body(max_reply_chars=99999))
        self.assertEqual(p.max_chars, 600)                  # clamped to the relay limit
        self.assertEqual(r.handle("Bearer " + TOKEN, body(max_reply_chars="lots"))[0], 400)

    def test_sanitize(self):
        text, cut = sanitize_reply("## Titolo\n**x** — “ok”", 100)
        self.assertEqual(text, "Titolo\nx - \"ok\"")
        self.assertFalse(cut)
        text, cut = sanitize_reply("a" * 50, 10)
        self.assertEqual(len(text), 10)
        self.assertTrue(cut)

    def test_config_file(self):
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
            json.dump({"devices": [{"name": "c", "token_sha256": HASH}], "rate_per_minute": 5}, fh)
        try:
            self.assertEqual(load_config(fh.name).rate_per_minute, 5)
        finally:
            os.unlink(fh.name)
        with self.assertRaises(ValueError):
            RelayConfig(devices=[Device("c", "short")]).validate()
        with self.assertRaises(ValueError):
            RelayConfig(devices=[]).validate()

    def test_rate_limiter_isolated_per_device(self):
        rl = RateLimiter(1, 10, clock=lambda: 0.0)
        self.assertIsNone(rl.check("a"))
        self.assertIsNotNone(rl.check("a"))
        self.assertIsNone(rl.check("b"))


class HttpServerTest(unittest.TestCase):
    def _post(self, url, data, token=TOKEN, ctx=None):
        req = urllib.request.Request(url, data=data, method="POST", headers={"Content-Type": "application/json"})
        if token:
            req.add_header("Authorization", "Bearer " + token)
        try:
            with urllib.request.urlopen(req, timeout=10, context=ctx) as resp:
                return resp.status, resp.read().decode(), dict(resp.headers)
        except urllib.error.HTTPError as e:
            return e.code, e.read().decode(), dict(e.headers)

    def _run(self, httpd):
        t = threading.Thread(target=httpd.serve_forever, daemon=True)
        t.start()
        self.addCleanup(httpd.shutdown)
        self.addCleanup(httpd.server_close)

    def test_plain_http_roundtrip(self):
        httpd = serve(config(), "127.0.0.1", 0, None, None, providers={"claude": FakeProvider("Il risultato e' 4")})
        self._run(httpd)
        url = f"http://127.0.0.1:{httpd.server_address[1]}"
        status, text, headers = self._post(url + "/v1/chat", body())
        self.assertEqual((status, text), (200, "Il risultato e' 4"))
        self.assertTrue(headers["Content-Type"].startswith("text/plain"))
        self.assertEqual(self._post(url + "/v1/chat", body(), token=None)[0], 401)
        self.assertEqual(self._post(url + "/nope", body())[0], 404)
        with urllib.request.urlopen(url + "/healthz", timeout=5) as r:
            self.assertEqual(r.read(), b"ok")

    def test_tls_with_certificate_verification(self):
        tmp = tempfile.mkdtemp()
        key, cert = os.path.join(tmp, "key.pem"), os.path.join(tmp, "cert.pem")
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", key, "-out", cert,
                        "-days", "1", "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1"],
                       check=True, capture_output=True)
        httpd = serve(config(), "127.0.0.1", 0, cert, key, providers={"claude": FakeProvider("ok-tls")})
        self._run(httpd)
        url = f"https://127.0.0.1:{httpd.server_address[1]}/v1/chat"
        trusted = ssl.create_default_context(cafile=cert)
        self.assertEqual(self._post(url, body(), ctx=trusted)[:2], (200, "ok-tls"))
        # A client that does not trust the relay certificate must refuse to connect.
        with self.assertRaises(urllib.error.URLError):
            self._post(url, body(), ctx=ssl.create_default_context(cafile=None, capath=tmp + "/none"))


class FakeApi(BaseHTTPRequestHandler):
    """Minimal stand-in for api.anthropic.com / api.openai.com."""
    responses = []          # list of (status, json) consumed in order
    requests = []

    def log_message(self, *a):
        pass

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        FakeApi.requests.append((self.path, dict(self.headers), json.loads(self.rfile.read(length))))
        status, payload = FakeApi.responses.pop(0)
        data = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


class ProviderAdapterTest(unittest.TestCase):
    def setUp(self):
        FakeApi.responses, FakeApi.requests = [], []
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), FakeApi)
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()
        self.base = f"http://127.0.0.1:{self.httpd.server_address[1]}"

    def tearDown(self):
        self.httpd.shutdown()
        self.httpd.server_close()

    def anthropic_provider(self):
        client = anthropic.Anthropic(api_key="sk-test", base_url=self.base, max_retries=0)
        return AnthropicProvider("claude-opus-5-5", "medium", 16000, 30, client=client)

    @staticmethod
    def message(content, stop="end_turn", **extra):
        m = {"id": "msg_1", "type": "message", "role": "assistant", "model": "claude-opus-5-5", "content": content,
             "stop_reason": stop, "stop_sequence": None, "usage": {"input_tokens": 12, "output_tokens": 3}}
        m.update(extra)
        return m

    def test_anthropic_request_shape_and_reply(self):
        FakeApi.responses = [(200, self.message([{"type": "thinking", "thinking": "", "signature": "s"},
                                                  {"type": "text", "text": "2+2 = 4"}]))]
        reply = self.anthropic_provider().chat([{"role": "user", "content": "2+2?"}])
        self.assertEqual((reply.text, reply.model, reply.truncated), ("2+2 = 4", "claude-opus-5-5", False))
        path, headers, sent = FakeApi.requests[0]
        self.assertTrue(path.startswith("/v1/messages"))
        headers = {k.lower(): v for k, v in headers.items()}
        # The SDK sends the key either as x-api-key or (anthropic>=2) as a Bearer token.
        self.assertTrue(headers.get("x-api-key") == "sk-test" or headers.get("authorization") == "Bearer sk-test")
        self.assertIn("anthropic-version", headers)
        self.assertIn("server-side-fallback-2026-07-01", headers["anthropic-beta"])
        self.assertEqual(sent["model"], "claude-opus-5-5")
        self.assertEqual(sent["fallbacks"], "default")
        self.assertEqual(sent["output_config"], {"effort": "medium"})
        self.assertEqual(sent["max_tokens"], 16000)
        self.assertIn("PicoCalc", sent["system"])
        self.assertNotIn("Limita", sent["system"])          # no length hint when not requested
        self.assertEqual(sent["messages"], [{"role": "user", "content": "2+2?"}])
        self.assertNotIn("thinking", sent)            # thinking is always on for Opus 5.5: not sent

    def test_anthropic_refusal_truncation_and_errors(self):
        FakeApi.responses = [(200, self.message([], stop="refusal", stop_details={"type": "refusal", "category": "cyber", "explanation": None}))]
        with self.assertRaises(RelayError) as cm:
            self.anthropic_provider().chat([{"role": "user", "content": "x"}])
        self.assertEqual(cm.exception.status, 422)
        FakeApi.responses = [(200, self.message([{"type": "text", "text": "lungo"}], stop="max_tokens"))]
        self.assertTrue(self.anthropic_provider().chat([{"role": "user", "content": "x"}]).truncated)
        cases = [(429, "rate_limit_error", 429), (401, "authentication_error", 502), (404, "not_found_error", 502),
                 (400, "invalid_request_error", 400), (529, "overloaded_error", 503), (500, "api_error", 503)]
        for http, kind, expected in cases:
            FakeApi.responses = [(http, {"type": "error", "error": {"type": kind, "message": "boom"}})]
            with self.assertRaises(RelayError) as cm:
                self.anthropic_provider().chat([{"role": "user", "content": "x"}])
            self.assertEqual(cm.exception.status, expected, kind)

    def test_anthropic_connection_error(self):
        client = anthropic.Anthropic(api_key="sk-test", base_url="http://127.0.0.1:9", max_retries=0, timeout=2)
        with self.assertRaises(RelayError) as cm:
            AnthropicProvider("claude-opus-5-5", "medium", 1000, 2, client=client).chat([{"role": "user", "content": "x"}])
        self.assertIn(cm.exception.status, (502, 504))

    def openai_provider(self, model="gpt-test-model"):
        client = openai.OpenAI(api_key="sk-test", base_url=self.base + "/v1", max_retries=0)
        return OpenAIProvider(model, 4000, 30, client=client)

    def test_openai_request_shape_and_errors(self):
        FakeApi.responses = [(200, {"id": "c1", "object": "chat.completion", "created": 0, "model": "gpt-test-model",
                                    "choices": [{"index": 0, "finish_reason": "stop",
                                                 "message": {"role": "assistant", "content": "Quattro", "refusal": None}}]})]
        reply = self.openai_provider().chat([{"role": "user", "content": "2+2?"}])
        self.assertEqual((reply.text, reply.model), ("Quattro", "gpt-test-model"))
        path, headers, sent = FakeApi.requests[0]
        self.assertEqual(path, "/v1/chat/completions")
        self.assertEqual({k.lower(): v for k, v in headers.items()}["authorization"], "Bearer sk-test")
        self.assertEqual(sent["messages"][0]["role"], "system")
        self.assertEqual(sent["messages"][1], {"role": "user", "content": "2+2?"})
        self.assertEqual(sent["max_completion_tokens"], 4000)
        FakeApi.responses = [(401, {"error": {"message": "bad key", "type": "invalid_request_error"}})]
        with self.assertRaises(RelayError) as cm:
            self.openai_provider().chat([{"role": "user", "content": "x"}])
        self.assertEqual(cm.exception.status, 502)
        with self.assertRaises(RelayError) as cm:
            self.openai_provider(model="").chat([{"role": "user", "content": "x"}])
        self.assertEqual(cm.exception.status, 503)


if __name__ == "__main__":
    unittest.main()
