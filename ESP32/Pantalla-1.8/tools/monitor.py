import serial, sys, time
port = sys.argv[1]; secs = float(sys.argv[2]) if len(sys.argv) > 2 else 6
s = serial.Serial()
s.port = port; s.baudrate = 115200; s.timeout = 0.2
s.dtr = False; s.rts = False   # ANTES de open(): no tocar DOWNLOAD/reset
s.open()
t0 = time.time(); buf = b''
while time.time() - t0 < secs:
    buf += s.read(4096)
s.close()
txt = buf.decode('utf-8', 'replace')
print(f'--- {len(buf)} bytes en {secs}s ---')
print(txt[-4000:] if txt else '(nada)')
