# Link — guida breve (v1.17.0, modalità Base)

Solo **Pimoroni Pico Plus 2 W**. MENU › **E**. Trasferimenti con un **PC**
sulla **USB seriale** con un **formato proprio** (il protocollo Link Casio tra
calcolatrici non è supportato). Stato **Base**.

## Sul dispositivo
- F1 **SEND** › ALL (il workspace) o FILE (un file dei tipi della calcolatrice:
  `.pceact`, `.pcsheet`, `.pcprog`, `.py`, `.pcgeo`, …).
- F2 **RECEIVE**: aspetta un pacchetto dal PC; arrivato, lo conserva in
  `PICOCALC/LINK/<NOME>.pclink` e ne mostra il contenuto; F1 **APPLY** scrive i
  file (scrittura atomica) e, per un workspace, chiede conferma prima di
  sostituire tutti i dati.
- F3 **PACKETS**: pacchetti ricevuti, APPLY o DELETE.
- Barra di avanzamento; **AC** annulla (il PC riceve ABORT).

## Sul PC: `tools/picocalc_link.py`
```bash
python3 tools/picocalc_link.py receive --port /dev/ttyACM0 -o workspace.pclink --extract ricevuti
python3 tools/picocalc_link.py send --port /dev/ttyACM0 conti.pcsheet hi.py:PICOCALC/PY/HI.py
python3 tools/picocalc_link.py list workspace.pclink
```
Senza dipendenze (porta seriale POSIX in modalità raw; usa pyserial se c'è).

## Protocollo
Trame `0x7E 'P' | tipo | seq u16 | lunghezza u16 (≤ 1024) | dati | CRC-32`,
confermate una per una (BEGIN, DATA…, END con il CRC-32 del pacchetto; ACK,
NAK, ABORT), ritrasmissione dopo 1 s fino a 5 volte, «Time Out» dopo 15 s di
silenzio. Le righe di log del firmware sulla stessa porta sono scartate dal
decodificatore. Formato del pacchetto e del file `.pclink`:
[FILE_FORMATS.md](FILE_FORMATS.md).
