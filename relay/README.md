# PicoCalc FX AI relay

Server HTTPS che collega la calcolatrice a Claude (Anthropic) e ChatGPT (OpenAI)
senza mettere chiavi API sul dispositivo.

- Chiavi API solo nell'ambiente: `ANTHROPIC_API_KEY`, `OPENAI_API_KEY`.
- Un token per calcolatrice; nel file di configurazione c'è solo il suo SHA-256.
- Limiti di dimensione e di frequenza per dispositivo; i contenuti dei messaggi
  non vengono registrati.
- SDK ufficiali: `anthropic` (modello predefinito `claude-opus-5-5`, fallback lato
  server sui rifiuti) e `openai` (modello scelto dall'operatore).

## Avvio rapido

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
cp relay.example.json relay.json
.venv/bin/python tools/new_device_token.py picocalc-1   # token per la calcolatrice + riga per relay.json
export ANTHROPIC_API_KEY=...
.venv/bin/python -m picocalc_relay --config relay.json --port 8443 --cert fullchain.pem --key privkey.pem
```

Test: `.venv/bin/python -m unittest discover -s tests -v` (nessuna chiave
necessaria: le API sono simulate).

Protocollo, sicurezza e configurazione completa:
[../docs/AI_INTEGRATION.md](../docs/AI_INTEGRATION.md) ·
[../docs/INSTALLATION.md](../docs/INSTALLATION.md#4-relay-ai).
