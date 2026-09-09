#!/usr/bin/env python3
"""
VNL-1 — Genera audio de prueba para validar el DFPlayer sin musica real.

Cada pista se anuncia sola: la pista 3 de la carpeta 2 suena "bip bip bip" y
luego un tono sostenido. Asi, de oido y sin ver la pantalla, sabes exactamente
que carpeta y que pista esta tocando el modulo — que es justo lo que hay que
comprobar de playFolder(carpeta, pista).

  python3 make_test_tones.py          # escribe en sd/src_test/01..03
"""
import subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT  = HERE / "src_test"

# Una frecuencia por carpeta: grave, media, aguda. Se distinguen sin esfuerzo.
FREQ   = {1: 392.0, 2: 587.0, 3: 880.0}
TRACKS = 3

BEEP_S, GAP_S, TONE_S = 0.16, 0.20, 3.0


def build(folder: int, track: int, dst: Path):
    f = FREQ[folder]
    args, pads = ["ffmpeg", "-nostdin", "-loglevel", "error", "-y"], []
    n = 0
    for _ in range(track):                      # tantos bips como numero de pista
        args += ["-f", "lavfi", "-i", f"sine=frequency={f}:duration={BEEP_S}"]
        pads.append(f"[{n}:a]"); n += 1
        args += ["-f", "lavfi", "-i",
                 f"anullsrc=channel_layout=mono:sample_rate=44100:duration={GAP_S}"]
        pads.append(f"[{n}:a]"); n += 1
    # Tono sostenido, una quinta abajo: marca el cuerpo de la "cancion".
    args += ["-f", "lavfi", "-i", f"sine=frequency={f * 0.67:.1f}:duration={TONE_S}"]
    pads.append(f"[{n}:a]"); n += 1

    args += ["-filter_complex",
             f"{''.join(pads)}concat=n={n}:v=0:a=1,volume=0.6[out]",
             "-map", "[out]", "-ar", "44100", "-ac", "1", str(dst)]
    subprocess.run(args, check=True)


def main():
    for folder in FREQ:
        d = OUT / f"{folder:02d}"
        d.mkdir(parents=True, exist_ok=True)
        for t in range(1, TRACKS + 1):
            dst = d / f"{t:02d} - carpeta {folder} pista {t}.wav"
            build(folder, t, dst)
            print(f"  {dst.relative_to(HERE)}")
    print(f"\nListo. Ahora:\n"
          f"  python3 prepare_sd.py --src src_test --out /Volumes/<TARJETA> --clean-card")


if __name__ == "__main__":
    sys.exit(main())
