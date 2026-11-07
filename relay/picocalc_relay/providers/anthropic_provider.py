"""Claude via the official Anthropic Python SDK (Messages API).

- Model: claude-opus-5-5 by default (configurable). Adaptive thinking is always
  on for this model, so depth is set with output_config.effort.
- Server-side refusal fallback is enabled (`fallbacks="default"`, beta
  server-side-fallback-2026-07-01); a final stop_reason "refusal" is reported
  to the device instead of empty content.
"""
from __future__ import annotations

import anthropic

from ..errors import RelayError
from . import Reply, system_prompt

FALLBACK_BETA = "server-side-fallback-2026-07-01"


class AnthropicProvider:
    def __init__(self, model: str, effort: str, max_tokens: int, timeout: float,
                 client: anthropic.Anthropic | None = None):
        self.model = model
        self.effort = effort
        self.max_tokens = max_tokens
        # API key from ANTHROPIC_API_KEY (or another SDK credential source); never from the device.
        self.client = client or anthropic.Anthropic(timeout=timeout, max_retries=2)

    def chat(self, messages: list[dict], model: str | None = None, max_chars: int | None = None) -> Reply:
        try:
            response = self.client.beta.messages.create(
                model=model or self.model,
                max_tokens=self.max_tokens,
                system=system_prompt(max_chars),
                messages=messages,
                output_config={"effort": self.effort},
                betas=[FALLBACK_BETA],
                fallbacks="default",
            )
        except anthropic.AuthenticationError:
            raise RelayError(502, "Relay: chiave API Anthropic non valida")
        except anthropic.PermissionDeniedError:
            raise RelayError(502, "Relay: chiave API Anthropic senza permessi per il modello")
        except anthropic.NotFoundError:
            raise RelayError(502, "Relay: modello Claude non trovato")
        except anthropic.RateLimitError:
            raise RelayError(429, "Claude e' occupato o hai superato il limite: riprova tra poco")
        except anthropic.BadRequestError as e:
            raise RelayError(400, f"Richiesta non valida per Claude: {e.message}"[:300])
        except anthropic.APIStatusError as e:
            if e.status_code >= 500:
                raise RelayError(503, f"Claude temporaneamente non disponibile ({e.status_code})")
            raise RelayError(502, f"Errore Claude ({e.status_code})")
        except anthropic.APITimeoutError:
            raise RelayError(504, "Timeout: Claude non ha risposto in tempo")
        except anthropic.APIConnectionError:
            raise RelayError(502, "Relay: Anthropic non raggiungibile")

        if response.stop_reason == "refusal":
            raise RelayError(422, "Richiesta rifiutata dai filtri di sicurezza del modello")
        text = "".join(block.text for block in response.content if block.type == "text").strip()
        if not text:
            raise RelayError(502, "Claude non ha restituito testo")
        return Reply(text=text, model=response.model, truncated=response.stop_reason == "max_tokens")
