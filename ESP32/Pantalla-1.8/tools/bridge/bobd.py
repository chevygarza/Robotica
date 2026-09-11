#!/usr/bin/env python3
"""bobd — demonio del puente Mac <-> Bob Aerogro.

Mantiene el puerto USB abierto (abrirlo resetea el chip en macOS, asi que solo se
abre UNA vez) y recibe eventos por un socket Unix. Cada linea recibida se manda
tal cual a Bob. El serial de Bob se guarda en un log.

  bobd.py                      # corre en primer plano
  echo "EV done" | nc -U /tmp/bob-aerogro.sock
"""
import glob, os, select, socket, sys, time

SOCK = "/tmp/bob-aerogro.sock"
LOG = os.path.expanduser("~/Library/Logs/bob-aerogro.log")

def find_port():
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    return ports[0] if ports else None

def open_serial(port):
    import serial
    s = serial.Serial()
    s.port = port; s.baudrate = 115200; s.timeout = 0
    s.dtr = False; s.rts = False   # antes de open(): no tocar DOWNLOAD/reset
    s.open()
    return s

def main():
    try: os.unlink(SOCK)
    except FileNotFoundError: pass
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(SOCK); srv.listen(8); srv.setblocking(False)
    os.chmod(SOCK, 0o666)
    log = open(LOG, "a", buffering=1)
    ser = None
    print(f"[bobd] socket {SOCK}, log {LOG}", flush=True)
    while True:
        if ser is None:
            port = find_port()
            if port:
                try:
                    ser = open_serial(port); print(f"[bobd] puerto {port}", flush=True)
                except Exception as e:
                    print(f"[bobd] no pude abrir {port}: {e}", flush=True); time.sleep(2)
            else:
                time.sleep(2)
            if ser is None:
                # aun sin puerto aceptamos clientes para no bloquearlos
                r, _, _ = select.select([srv], [], [], 0.5)
                for _ in r:
                    c, _ = srv.accept(); c.close()
                continue
        r, _, _ = select.select([srv], [], [], 0.05)
        for _ in r:
            c, _ = srv.accept()
            c.settimeout(0.5)
            try:
                data = c.recv(512).decode("utf-8", "replace").strip()
                if data == "__pause__":   # build.sh va a flashear: soltar el puerto
                    if ser: ser.close(); ser = None
                    c.sendall(b"ok\n"); c.close(); print("[bobd] puerto liberado (flash)", flush=True)
                    time.sleep(1)
                    # espera a que el puerto vuelva y reabre
                    for _ in range(120):
                        time.sleep(1)
                        p = find_port()
                        if p:
                            try: ser = open_serial(p); print(f"[bobd] puerto reabierto {p}", flush=True); break
                            except Exception: pass
                    continue
                if data and ser:
                    for line in data.splitlines():
                        ser.write((line.strip() + "\n").encode()); log.write(f"> {line.strip()}\n")
                    c.sendall(b"ok\n")
            except Exception as e:
                print(f"[bobd] cliente: {e}", flush=True)
            finally:
                try: c.close()
                except Exception: pass
        try:
            chunk = ser.read(4096)
            if chunk: log.write(chunk.decode("utf-8", "replace"))
        except Exception as e:
            print(f"[bobd] serial perdido: {e}", flush=True)
            try: ser.close()
            except Exception: pass
            ser = None

if __name__ == "__main__":
    main()
