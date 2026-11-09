"""ChatGPT via the official OpenAI Python SDK (Chat Completions).

The model must be configured by the relay operator (openai_model) to a model
enabled on their OpenAI account; the key comes from OPENAI_API_KEY.
"""
from __future__ import annotations

import openai

from ..errors import RelayError
from . import Reply, system_prompt


class OpenAIProvider:
    def __init__(self, model: str, max_tokens: int, timeout: float, client: openai.OpenAI | None = None):
        self.model = model
        self.max_tokens = max_tokens
        self.client = client or openai.OpenAI(timeout=timeout, max_retries=2)

    def chat(self, messages: list[dict], model: str | None = None, max_chars: int | None = None) -> Reply:
        target = model or self.model
        if not target:
            raise RelayError(503, "ChatGPT non configurato sul relay (openai_model)")
        try:
            response = self.client.chat.completions.create(
                model=target,
                messages=[{"role": "system", "content": system_prompt(max_chars)}, *messages],
                max_completion_tokens=self.max_tokens,
            )
        except openai.AuthenticationError:
            raise RelayError(502, "Relay: chiave API OpenAI non valida")
        except openai.PermissionDeniedError:
            raise RelayError(502, "Relay: chiave API OpenAI senza permessi per il modello")
        except openai.NotFoundError:
            raise RelayError(502, "Relay: modello ChatGPT non trovato")
        except openai.RateLimitError:
            raise RelayError(429, "ChatGPT e' occupato o hai superato il limite: riprova tra poco")
        except openai.BadRequestError as e:
            raise RelayError(400, f"Richiesta non valida per ChatGPT: {e.message}"[:300])
        except openai.APIStatusError as e:
            if e.status_code >= 500:
                raise RelayError(503, f"ChatGPT temporaneamente non disponibile ({e.status_code})")
            raise RelayError(502, f"Errore ChatGPT ({e.status_code})")
        except openai.APITimeoutError:
            raise RelayError(504, "Timeout: ChatGPT non ha risposto in tempo")
        except openai.APIConnectionError:
            raise RelayError(502, "Relay: OpenAI non raggiungibile")

        if not response.choices:
            raise RelayError(502, "ChatGPT non ha restituito risposte")
        choice = response.choices[0]
        if getattr(choice.message, "refusal", None):
            raise RelayError(422, "Richiesta rifiutata dal modello")
        text = (choice.message.content or "").strip()
        if not text:
            raise RelayError(502, "ChatGPT non ha restituito testo")
        return Reply(text=text, model=response.model, truncated=choice.finish_reason == "length")
