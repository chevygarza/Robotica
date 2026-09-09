import sys, time, serial

PORT = "/dev/cu.usbmodem11401"
BAUD = 115200
SECONDS = 12

ser = serial.Serial(PORT, BAUD, timeout=0.2)

# Reset normal de la placa (pulso EN/RTS). No borra nada, solo reinicia.
ser.setDTR(False)
ser.setRTS(True)
time.sleep(0.2)
ser.setRTS(False)
ser.setDTR(False)

start = time.time()
buf = bytearray()
while time.time() - start < SECONDS:
    data = ser.read(4096)
    if data:
        buf += data
        sys.stdout.buffer.write(data)
        sys.stdout.flush()
ser.close()

sys.stderr.write(f"\n\n[capturados {len(buf)} bytes en {SECONDS}s]\n")
