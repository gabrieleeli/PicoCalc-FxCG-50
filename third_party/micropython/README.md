# MicroPython per la modalità Python (v1.17.0, solo Pimoroni Pico Plus 2 W)

- `micropython_embed/`: pacchetto generato dalla porta **embed** di
  MicroPython **v1.24.1** (licenza MIT, `LICENSE`), non modificato, tranne
  `port/mphalport.c` tolto (l'I/O è in `usermod/picocalc/pyglue.c`).
- `mpconfigport.h`: configurazione del progetto (ROM level CORE_FEATURES,
  double, interi lunghi, `math`, `sys`, KeyboardInterrupt, hook della VM per il
  lavoro a passi, `input()` della PicoCalc).
- `usermod/picocalc/`: codice del progetto in C (modulo `casioplot` minimo,
  colla verso `src/python_port.cpp`).

Rigenerare il pacchetto (serve Python 3 e un checkout di MicroPython v1.24.1):

```bash
git clone --depth 1 --branch v1.24.1 https://github.com/micropython/micropython.git /tmp/micropython
make -C third_party/micropython MICROPYTHON_TOP=/tmp/micropython
rm -rf third_party/micropython/build-embed third_party/micropython/micropython_embed/port/mphalport.c
```
