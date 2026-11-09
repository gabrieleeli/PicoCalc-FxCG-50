"""Relay configuration (JSON file). Secrets are never stored here:
- provider API keys come from ANTHROPIC_API_KEY / OPENAI_API_KEY in the environment;
- device tokens are stored only as SHA-256 hex digests (see tools/new_device_token.py).
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field

HEX64 = re.compile(r"^[0-9a-f]{64}$")


@dataclass
class Device:
    name: str
    token_sha256: str


@dataclass
class RelayConfig:
    devices: list[Device] = field(default_factory=list)
    # Claude: Opus 5.5 by default; the operator can change it here.
    anthropic_model: str = "claude-opus-5-5"
    anthropic_effort: str = "medium"           # low | medium | high | xhigh | max
    anthropic_max_tokens: int = 16000          # includes adaptive thinking tokens
    # ChatGPT: set to a model enabled on your OpenAI account (left empty = ChatGPT disabled).
    openai_model: str = ""
    openai_max_tokens: int = 4000
    # Models a device may request explicitly (empty = only the defaults above).
    allowed_models: dict[str, list[str]] = field(default_factory=dict)
    max_request_bytes: int = 16384
    max_messages: int = 20
    max_message_chars: int = 4000
    max_reply_chars: int = 4000
    rate_per_minute: int = 10
    rate_per_day: int = 300
    request_timeout_s: float = 120.0

    def validate(self) -> None:
        if not self.devices:
            raise ValueError("config: no devices configured")
        for d in self.devices:
            if not d.name or not HEX64.match(d.token_sha256):
                raise ValueError(f"config: device {d.name!r} needs a 64-char lowercase sha256 hex digest")
        if self.anthropic_effort not in {"low", "medium", "high", "xhigh", "max"}:
            raise ValueError("config: anthropic_effort must be low|medium|high|xhigh|max")
        for name in ("max_request_bytes", "max_messages", "max_message_chars", "max_reply_chars",
                     "rate_per_minute", "rate_per_day", "anthropic_max_tokens", "openai_max_tokens"):
            if getattr(self, name) <= 0:
                raise ValueError(f"config: {name} must be positive")


def load_config(path: str) -> RelayConfig:
    with open(path, encoding="utf-8") as fh:
        raw = json.load(fh)
    devices = [Device(d["name"], d["token_sha256"].lower()) for d in raw.pop("devices", [])]
    known = set(RelayConfig.__dataclass_fields__) - {"devices"}
    unknown = set(raw) - known
    if unknown:
        raise ValueError(f"config: unknown keys {sorted(unknown)}")
    cfg = RelayConfig(devices=devices, **raw)
    cfg.validate()
    return cfg
