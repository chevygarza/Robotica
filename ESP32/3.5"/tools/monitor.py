#!/usr/bin/env python3
"""Monitor serial del TAC-1 SIN tocar DTR/RTS.

Regla 7 de CROWN32: togglear DTR/RTS deja al S3 en modo DOWNLOAD (pantalla
negra, revivir = desconectar el USB). Se abre el puerto con las dos lineas
en False ANTES de open().  Uso: tools/.venv/bin/python tools/monitor.py [seg]
"""
import glob, sys, time, serial

port = (glob.glob("/dev/cu.usbmodem*") or [None])[0]
if not port:
    sys.exit("no hay puerto usbmodem")
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 0
s = serial.Serial()
s.port, s.baudrate, s.timeout = port, 115200, 0.2
s.dtr = False
s.rts = False
s.open()
t0 = time.time()
try:
    while not secs or time.time() - t0 < secs:
        line = s.readline()
        if line:
            sys.stdout.write(line.decode(errors="replace"))
            sys.stdout.flush()
except KeyboardInterrupt:
    pass
