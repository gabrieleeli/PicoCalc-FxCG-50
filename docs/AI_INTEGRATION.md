# Integrazione AI (Claude e ChatGPT) — v1.6.0

## Modello di sicurezza

```
PicoCalc ──Wi-Fi + HTTPS (TLS 1.2, certificato verificato)──▶ relay ──HTTPS──▶ Anthropic / OpenAI
          Authorization: Bearer <token del dispositivo>          chiavi API solo qui
```

- **Nessuna chiave API sul dispositivo**, nel firmware, nel repository o nei log.
  Le chiavi stanno solo nell'ambiente del relay (`ANTHROPIC_API_KEY`,
  `OPENAI_API_KEY`), lette dagli SDK ufficiali.
- Ogni calcolatrice ha un **token proprio**, generato con
  `relay/tools/new_device_token.py`. Il relay conserva solo l'hash SHA-256
  (`relay.json`) e lo confronta in tempo costante; un token si revoca togliendo la
  riga dal file.
- Il token e la password Wi-Fi sono salvati nella flash della calcolatrice (area
  workspace) e mascherati nell'interfaccia; chi ha accesso fisico al dispositivo
  può leggerli, per questo il token è revocabile e non dà accesso alle chiavi.
- Il firmware usa TLS con **verifica obbligatoria** del certificato
  (`ALTCP_MBEDTLS_AUTHMODE = MBEDTLS_SSL_VERIFY_REQUIRED`; il default di lwIP sarebbe
  `VERIFY_OPTIONAL`) e controllo del nome host (`mbedtls_ssl_set_hostname`). Le CA
  fidate sono compilate nel firmware da `certs/relay_ca.pem` (ISRG Root X1/X2, cioè
  Let's Encrypt) oppure da un file scelto con `-DFXAI_RELAY_CA_FILE=...`.
- Il relay non registra i contenuti dei messaggi, solo metadati (dispositivo,
  provider, stato, latenza); applica limiti di dimensione e di frequenza per
  dispositivo.
- Senza rete o relay tutte le funzioni matematiche restano disponibili.

## Protocollo calcolatrice → relay

```
POST /v1/chat
Authorization: Bearer <token>
Content-Type: application/json

{"provider": "claude" | "chatgpt",
 "model": "",                       // vuoto = modello predefinito del relay
 "max_reply_chars": 1200,
 "messages": [{"role": "user" | "assistant", "content": "..."}]}
```

Risposta `200 text/plain; charset=utf-8` con il testo dell'assistente (accorciato a
`max_reply_chars`) e intestazioni `X-Relay-Model`, `X-Relay-Truncated`. Gli errori
sono `4xx/5xx text/plain` con una riga leggibile (401 token, 413 richiesta troppo
grande, 422 rifiuto del modello, 429 limite di frequenza, 502/503/504 problemi del
provider). `GET /healthz` restituisce `ok`.

## Relay (`relay/`)

| File | Ruolo |
|---|---|
| `picocalc_relay/server.py` | server HTTP(S) a thread, autenticazione, limiti, instradamento |
| `picocalc_relay/config.py` | configurazione JSON validata (chiavi sconosciute rifiutate) |
| `picocalc_relay/ratelimit.py` | limiti al minuto e al giorno per dispositivo |
| `picocalc_relay/providers/anthropic_provider.py` | Claude con l'SDK ufficiale `anthropic` |
| `picocalc_relay/providers/openai_provider.py` | ChatGPT con l'SDK ufficiale `openai` |
| `tools/new_device_token.py` | genera un token e la riga per `relay.json` |
| `tests/test_relay.py` | 15 test: SDK reali contro API simulate, TLS in loopback |

### Claude

- Modello predefinito `claude-opus-5-5` (configurabile con `anthropic_model`; i
  modelli che il dispositivo può chiedere esplicitamente sono in `allowed_models`).
- `output_config.effort` (predefinito `medium`) regola la profondità del
  ragionamento; `anthropic_max_tokens` (16 000) include il ragionamento.
- Fallback lato server sui rifiuti abilitato (`betas=["server-side-fallback-2026-07-01"]`,
  `fallbacks="default"`); se la risposta finale è comunque un rifiuto
  (`stop_reason == "refusal"`) il dispositivo riceve un messaggio esplicito.
- Prompt di sistema: risposte concise in testo semplice e notazione lineare,
  adatte a uno schermo di 40 colonne.

### ChatGPT

- SDK ufficiale `openai`, Chat Completions con `max_completion_tokens`.
- Il modello va impostato dall'operatore (`openai_model`) con un modello
  abilitato sul proprio account; se è vuoto ChatGPT risulta disattivato.

### Configurazione (`relay.json`)

Vedi `relay/relay.example.json`: `devices` (nome + `token_sha256`), modelli,
`max_request_bytes` (16 KB), `max_messages` (20), `max_message_chars`,
`max_reply_chars`, `rate_per_minute` (10), `rate_per_day` (300),
`request_timeout_s` (120).

## Firmware

| Componente | Dettagli |
|---|---|
| `src/platform/pico/net_pico.cpp` | Wi-Fi CYW43 (WPA2 o rete aperta), DNS, TCP + TLS (lwIP altcp_tls + mbedTLS), macchina a stati non bloccante: Wi-Fi → DNS → connessione → invio → ricezione; timeout 30 s Wi-Fi e 120 s totale |
| `src/ai_protocol.cpp` | corpo JSON (storia accorciata a 12 KB), richiesta HTTP/1.1, parser della risposta (anche chunked), pulizia del testo |
| `src/apps/ai_chat_app.cpp` | conversazione scorrevole, provider Claude/ChatGPT, stato della rete, annullamento |
| `include/net.hpp` | interfaccia `ChatTransport` (sostituita da un trasporto simulato nei test) |

Dettagli lwIP: scritture TLS a blocchi da ≤1024 byte (lwIP non accetta scritture
parziali di `mbedtls_ssl_write`), chiusura della connessione solo dal ciclo
`poll()` e mai dentro le callback, `MEM_LIBC_MALLOC` per usare l'heap comune.
Risposta massima 16 KB, conversazione salvata fino a 30 messaggi.

## Stato delle prove

- Protocollo, parser HTTP, interfaccia della chat: **TESTED ON HOST** (`ai_test`, 138 controlli).
- Relay: **TESTED ON HOST** con API simulate e TLS reale in loopback.
- Wi-Fi, DNS, handshake TLS e consumo di RAM di mbedTLS sul Pico: **non ancora
  provati su hardware**.
- Chiamate reali ad Anthropic/OpenAI: **non provate** (richiedono chiavi e credito
  dell'utente).

Installazione del relay: [INSTALLATION.md](INSTALLATION.md#4-relay-ai).
