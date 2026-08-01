#!/usr/bin/env python3
"""
VNL-1 — Prepara la microSD del DFPlayer y regenera el manifiesto.

Toma los audios que pongas en src/01, src/02, src/03 (cualquier formato que
ffmpeg lea), los convierte al MP3 que el DFPlayer digiere sin quejarse, los
nombra 001.mp3 ... 0NN.mp3 en orden, y saca la duracion EXACTA de cada pista
con ffprobe para escribir albums_gen.h.

  python3 prepare_sd.py                        # prueba en seco, no escribe audio
  python3 prepare_sd.py --out .                # arma sd/01, sd/02, sd/03
  python3 prepare_sd.py --out /Volumes/VNL1    # escribe directo en la tarjeta

Dos cosas que arruinan una SD de DFPlayer y que este script cuida:

1) El modulo NO ordena por nombre de archivo, ordena por el orden de entrada en
   la tabla FAT. Por eso los archivos se copian uno por uno, en secuencia.
2) macOS escribe metadata invisible (.DS_Store, ._001.mp3, .Spotlight-V100) y el
   DFPlayer las cuenta como pistas: te toca silencio donde deberia ir musica.
   --clean-card las borra al final.
"""
import argparse, json, os, re, shutil, subprocess, sys
from pathlib import Path

# Debe reflejar albums.h. El script escribe las duraciones; lo demas es de aqui.
ALBUMS = [
    {"folder": 1, "name": "FIFA",    "subtitle": "Mejores canciones", "color": "0x1DB954"},
    {"folder": 2, "name": "ZELDA",   "subtitle": "Mejores canciones", "color": "0x3A6FD8"},
    {"folder": 3, "name": "NATURAL", "subtitle": "Sonidos naturales", "color": "0xC8791E"},
]

AUDIO_EXT = {".mp3", ".m4a", ".aac", ".wav", ".flac", ".ogg", ".opus", ".aiff", ".wma"}
JUNK = {".DS_Store", ".Spotlight-V100", ".fseventsd", ".Trashes", "._.Trashes"}


def natural_key(p: Path):
    """Ordena '2 - x.mp3' antes de '10 - y.mp3', como espera cualquier humano."""
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", p.name)]


def duration_s(path: Path) -> int:
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "json", str(path)],
        capture_output=True, text=True, check=True).stdout
    return int(round(float(json.loads(out)["format"]["duration"])))


def convert(src: Path, dst: Path):
    # 192kbps CBR, 44.1kHz estereo: dentro de lo que el DFPlayer soporta con
    # margen, y sin VBR (algunos lotes se atragantan con VBR al buscar).
    subprocess.run(
        ["ffmpeg", "-nostdin", "-loglevel", "error", "-y", "-i", str(src),
         "-map", "0:a:0", "-c:a", "libmp3lame", "-b:a", "192k",
         "-ar", "44100", "-ac", "2", "-write_xing", "0", str(dst)],
        check=True)


def clean_card(root: Path):
    removed = 0
    for p in sorted(root.rglob("*"), key=lambda x: -len(x.parts)):
        if p.name in JUNK or p.name.startswith("._"):
            shutil.rmtree(p, ignore_errors=True) if p.is_dir() else p.unlink(missing_ok=True)
            removed += 1
    print(f"  metadata de macOS borrada: {removed} entradas")


def main():
    ap = argparse.ArgumentParser()
    here = Path(__file__).resolve().parent
    ap.add_argument("--src", default=str(here / "src"),
                    help="carpeta con src/01, src/02, src/03")
    ap.add_argument("--out", default=None,
                    help="destino: la carpeta sd/ o el punto de montaje de la tarjeta")
    ap.add_argument("--manifest", default=str(here.parent / "firmware/VNL1/albums_gen.h"))
    ap.add_argument("--clean-card", action="store_true")
    args = ap.parse_args()

    src = Path(args.src).expanduser()
    out = Path(args.out).expanduser() if args.out else None
    if out is None:
        print("Prueba en seco: no se escribe audio. Usa --out para hacerlo real.\n")

    manifest = []
    for alb in ALBUMS:
        folder = f"{alb['folder']:02d}"
        sdir = src / folder
        files = []
        if sdir.is_dir():
            files = sorted([p for p in sdir.iterdir()
                            if p.suffix.lower() in AUDIO_EXT], key=natural_key)

        print(f"/{folder}  {alb['name']:<8} {len(files)} archivo(s)")
        if not files:
            print(f"         (vacio: pon los audios en {sdir})")
            manifest.append({**alb, "durations": []})
            continue

        if len(files) > 255:
            print("         AVISO: el DFPlayer topa en 255 pistas por carpeta")
            files = files[:255]

        durations = []
        odir = (out / folder) if out else None
        if odir:
            odir.mkdir(parents=True, exist_ok=True)

        # Uno por uno y en orden: el DFPlayer sigue el orden de la tabla FAT.
        for i, f in enumerate(files, start=1):
            target = odir / f"{i:03d}.mp3" if odir else None
            if target:
                convert(f, target)
                d = duration_s(target)
            else:
                d = duration_s(f)
            durations.append(d)
            print(f"         {i:03d}.mp3  {d // 60}:{d % 60:02d}  {f.name}")

        total = sum(durations)
        print(f"         total {total // 60} min {total % 60} s")
        manifest.append({**alb, "durations": durations})

    if out and args.clean_card:
        print("\nLimpiando la tarjeta:")
        clean_card(out)

    # ── Manifiesto ───────────────────────────────────────────────────────────
    lines = [
        "// VNL-1 — GENERADO por sd/prepare_sd.py. No editar a mano.",
        "// Duraciones reales de cada archivo, sacadas con ffprobe: es lo que",
        "// permite una barra de progreso honesta (el DFPlayer no da metadata).",
        "#pragma once",
        "#include <Arduino.h>",
        "",
        "struct AlbumGen {",
        "  uint8_t         folder;",
        "  const char*     name;",
        "  const char*     subtitle;",
        "  uint8_t         tracks;",
        "  uint16_t        seconds;        // total del album",
        "  const uint16_t* track_seconds;  // duracion de cada pista",
        "  uint32_t        color;",
        "};",
        "",
    ]
    for alb in manifest:
        d = alb["durations"]
        lines.append(f"static const uint16_t TRACKS_{alb['folder']:02d}[] = "
                     f"{{ {', '.join(str(x) for x in d) if d else '0'} }};")
    lines += ["", "static const AlbumGen ALBUMS_GEN[] = {"]
    for alb in manifest:
        d = alb["durations"]
        lines.append(f'  {{ {alb["folder"]}, "{alb["name"]}", "{alb["subtitle"]}", '
                     f'{len(d)}, {sum(d)}, TRACKS_{alb["folder"]:02d}, {alb["color"]} }},')
    lines += ["};", "",
              "static const uint8_t ALBUM_GEN_COUNT = "
              "sizeof(ALBUMS_GEN) / sizeof(ALBUMS_GEN[0]);", ""]

    Path(args.manifest).write_text("\n".join(lines))
    print(f"\nmanifiesto escrito: {args.manifest}")


if __name__ == "__main__":
    sys.exit(main())
