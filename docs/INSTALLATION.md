# Installazione — v1.9.0

> Il firmware non è ancora stato provato su una PicoCalc fisica (né con il
> Pimoroni Pico Plus 2 W né con il Pico 2 W). Prima di usarlo, conservare una
> copia del firmware attuale della propria scheda (se presente) e seguire il
> collaudo in [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md).

## 1. Requisiti

- ClockworkPi **PicoCalc** (mainboard v2.0) con uno dei due moduli:
  - **Pimoroni Pico Plus 2 W** (RP2350B, 8 MB di PSRAM, 16 MB di flash) —
    scheda principale;
  - **Raspberry Pi Pico 2 W** (RP2350A, 4 MB di flash) — profilo alternativo,
    con i limiti di memoria della v1.8.0.
- Cavo USB dati per il modulo.
- Per l'AI: una rete Wi-Fi a 2,4 GHz (WPA2 o aperta) e un relay raggiungibile in
  HTTPS (sezione 4).

## 2. Installare l'UF2

| Modulo | File |
|---|---|
| Pimoroni Pico Plus 2 W | `picocalc_fxai_pico_plus_2w.uf2` |
| Raspberry Pi Pico 2 W | `picocalc_fxai_pico2w.uf2` |

1. Spegnere la PicoCalc.
2. Tenere premuto **BOOT/BOOTSEL** sul modulo e collegarlo al computer via USB:
   compare l'unità `RP2350`.
3. Copiare l'UF2 **del proprio modulo** sull'unità: il modulo si riavvia nel firmware.
4. In alternativa: `picotool load -x <file>.uf2`.

Gli UF2 non sono intercambiabili. Un'immagine Plus copiata su un Pico 2 W viene
riconosciuta all'avvio (capacità reale della flash): mostra "System ERROR: UF2
per … su una flash da 4 MB" e non scrive nulla oltre il chip, quindi basta
copiare l'UF2 giusto. Un'immagine Pico 2 W su una Plus usa gli stessi pin
(PicoCalc e modulo Wi-Fi) ma resta senza PSRAM, con i limiti e i primi 4 MB
di flash del Pico 2 W: configurazione non consigliata e non provata.

Controllo dell'immagine prima della copia:

```bash
picotool info -a picocalc_fxai_pico_plus_2w.uf2   # "PicoCalc FX-CG50 AI", versione 1.9.0 (pico_plus_2w)
sha256sum -c SHA256SUMS                            # se si usa il pacchetto di release
```

Il workspace (impostazioni, variabili, liste, storia, grafici, 3D, conversazioni
AI) è in un'area della flash che l'aggiornamento non tocca:

- **Pico Plus 2 W**: 1 MB prima dell'ultimo settore; i file interni (progetti e
  immagini, se in SYSTEM → "Archivio file" è scelta la flash interna) sono nella
  partizione LittleFS da ≈ 11 MB. Se sulla stessa Plus girava una v1.8.0, il
  suo workspace viene importato al primo avvio.
- **Pico 2 W**: gli ultimi 128 KB, come la v1.8.0: aggiornando da una v1.8.0
  i dati restano.

Per ripartire da zero: SYSTEM → F5 (reset, conferma con EXE); Memory permette
invece di cancellare singole categorie (storia, variabili, liste, funzioni,
funzioni 3D, Equation, chat).

## 3. Compilare dai sorgenti

Requisiti verificati: Ubuntu 24.04, `gcc-arm-none-eabi` 13.2, CMake ≥ 3.13, Ninja,
Pico SDK 2.2.0 (serve ≥ 2.1 per la scheda `pimoroni_pico_plus2_w_rp2350`) con i
submodule (tinyusb, cyw43-driver, lwip, mbedtls), Python 3.

```bash
git clone --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git
git -C pico-sdk submodule update --init lib/tinyusb lib/cyw43-driver lib/lwip lib/mbedtls
export PICO_SDK_PATH=$PWD/pico-sdk
```

```bash
scripts/build.sh --board pico_plus_2w   # -> build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2
scripts/build.sh --board pico2w         # -> build/pico2w/picocalc_fxai_pico2w.uf2
```

`scripts/build_pico2w.sh` resta come scorciatoia per il Pico 2 W. Ogni scheda ha
la sua cartella di build: CMake rifiuta di cambiare scheda in una cartella già
configurata.

Opzioni CMake:

| Opzione | Effetto |
|---|---|
| `FXAI_BOARD=PICO_PLUS_2W` / `PICO2W` | scheda (`--board` di `scripts/build.sh`) |
| `FXAI_OPT_LEVEL=O2` | ottimizzazione (`O2` predefinito, `O3`, `Os`) |
| `FXAI_FRAMEBUFFER_IN_PSRAM=ON` | framebuffer in PSRAM (solo Plus; vedi [PICO_PLUS_2W.md](PICO_PLUS_2W.md)) |
| `FXAI_ENABLE_BT=ON` | BTstack LE (nessuna funzione lo usa) |
| `FXAI_ENABLE_LTO=ON` | LTO dei sorgenti dell'applicazione (sperimentale) |
| `-DFXAI_DISPLAY_CALIBRATION_SCREEN=ON` | schermata di prova colori all'avvio |
| `-DFXAI_LCD_RGB565=ON` | invio a 16 bit (COLMOD 0x55) invece di RGB666 |
| `-DFXAI_LCD_SPI_HZ=25000000` | clock SPI del display |
| `-DFXAI_RELAY_CA_FILE=/percorso/ca.pem` | CA accettate per il relay (predefinito `certs/relay_ca.pem`, Let's Encrypt) |

Test su PC: `tests/run_host_tests.sh` (entrambi i profili di scheda; vedi [TESTING.md](TESTING.md)).
Pacchetto di release (zip dei sorgenti + i due UF2 + SHA256SUMS): `scripts/make_release.sh`,
dopo aver compilato entrambe le schede.

## 4. Relay AI

Il relay custodisce le chiavi API: va eseguito su un computer o server proprio,
raggiungibile dalla calcolatrice in HTTPS con un certificato valido per il suo
nome host.

### 4.1 Installazione

```bash
cd relay
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
cp relay.example.json relay.json
```

### 4.2 Token della calcolatrice

```bash
.venv/bin/python tools/new_device_token.py picocalc-1
```

Lo script stampa il token (da inserire **solo** nella calcolatrice) e la riga
`{"name": ..., "token_sha256": ...}` da mettere in `devices` dentro `relay.json`.
Per revocare un dispositivo basta togliere la sua riga.

### 4.3 Chiavi API e modelli

Le chiavi si passano come variabili d'ambiente, mai nei file di configurazione:

```bash
export ANTHROPIC_API_KEY=...           # Claude
export OPENAI_API_KEY=...              # ChatGPT (facoltativo)
```

In `relay.json`: `anthropic_model` (predefinito `claude-opus-5-5`),
`anthropic_effort`, `openai_model` (vuoto = ChatGPT disattivato; indicare un
modello abilitato sul proprio account OpenAI), limiti di dimensione e frequenza.

### 4.4 Avvio con TLS

Con un certificato proprio (es. Let's Encrypt):

```bash
.venv/bin/python -m picocalc_relay --config relay.json --port 8443 \
    --cert /etc/letsencrypt/live/relay.example.org/fullchain.pem \
    --key  /etc/letsencrypt/live/relay.example.org/privkey.pem
```

Oppure dietro un reverse proxy TLS (Caddy, nginx) che inoltra a
`--host 127.0.0.1 --port 8080` senza `--cert/--key`: in questo caso il relay non
deve essere esposto direttamente in rete.

Verifica: `curl https://relay.example.org:8443/healthz` → `ok`.

Se il certificato del relay non è emesso da Let's Encrypt, ricompilare il firmware
con `-DFXAI_RELAY_CA_FILE=` che punta alla CA corretta: la calcolatrice rifiuta
certificati non verificabili.

### 4.5 Configurazione della calcolatrice

SYSTEM (tasto `G` nel menu) → campi:

| Campo | Valore |
|---|---|
| Wi-Fi SSID / Wi-Fi password | rete a 2,4 GHz |
| Relay host | nome host del certificato, es. `relay.example.org` |
| Relay porta | 443 o la porta scelta (es. 8443) |
| Relay path | `/v1/chat` |
| Token relay | il token generato al punto 4.2 |
| AI provider | Claude o ChatGPT |
| AI modello | vuoto = predefinito del relay |
| AI max caratteri | 200–4000 |

Poi aprire **AI Chat** (tasto `M`). La riga di stato mostra lo stato del Wi-Fi;
gli errori del relay (token, limiti, rifiuti) compaiono come messaggi leggibili.

## 5. Tasti principali

Vedi la tabella nel [README](../README.md#tastiera-picocalc--fx-cg50). Nel menu
principale `1`–`9` e `A`–`N` aprono la modalità; **Esc tenuto premuto** torna al
menu da qualunque schermata.
