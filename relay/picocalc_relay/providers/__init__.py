"""Provider adapters. Each adapter lives in its own module and uses that
provider's official SDK; both expose chat(messages, model) -> Reply."""
from __future__ import annotations

from dataclasses import dataclass

SYSTEM_PROMPT = (
    "Sei l'assistente matematico di una calcolatrice grafica PicoCalc (schermo 320x320, "
    "font monospazio di 40 colonne). Rispondi nella lingua dell'utente, in modo conciso e "
    "corretto. Usa solo testo semplice: niente Markdown, niente LaTeX, niente tabelle. "
    "Scrivi le formule in notazione lineare (es. x^2, sqrt(2), integral(x^2,0,1)). "
    "Per i calcoli mostra i passaggi essenziali e verifica il risultato."
)


def system_prompt(max_chars: int | None) -> str:
    if max_chars:
        return SYSTEM_PROMPT + f" Limita la risposta a circa {max_chars} caratteri."
    return SYSTEM_PROMPT


@dataclass
class Reply:
    text: str
    model: str
    truncated: bool = False
