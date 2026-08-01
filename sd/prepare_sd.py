#!/usr/bin/env python3
"""
VNL-1 — Prepara la microSD del DFPlayer y regenera el manifiesto del firmware.

Las carpetas de origen se llaman "NN Nombre" y ese nombre es el que aparece en
la pantalla del reproductor:

    sd/src/01 FIFA/
    sd/src/02 Zelda/
    sd/src/03 Sonidos naturales/
    ...
    sd/src/20 Lo que sea/

El numero manda a que carpeta de la tarjeta va (el DFPlayer SOLO entiende
carpetas numericas: /01, /02, ...). El nombre es para ti y para la UI.

Opcional: un archivo album.txt dentro de la carpeta con

    subtitulo = Mejores canciones
    color     = 0x1DB954

Si no hay color, se asigna uno de la paleta segun el numero de carpeta.

    python3 prepare_sd.py                                  # prueba en seco
    python3 prepare_sd.py --out /Volumes/<TARJETA> --clean-card

Cada corrida reescribe firmware/VNL1/albums.h con los datos REALES: numero de
pistas, duracion de cada una medida con ffprobe y duracion total. Agregar el
disco 20 es crear su carpeta, correr esto y reflashear. Nada mas.

Dos trampas del DFPlayer que este script cubre:

1) NO ordena por nombre de archivo, sino por el orden de la tabla FAT. Por eso
   los archivos se copian uno por uno, en secuencia.
2) macOS escribe metadata invisible (.DS_Store, ._001.mp3, .Spotlight-V100) y
   el modulo la cuenta como pistas: te toca silencio donde deberia ir musica.
   --clean-card la borra al final.
"""
import argparse, json, os, re, shutil, subprocess, sys
from pathlib import Path

AUDIO_EXT = {".mp3", ".m4a", ".aac", ".wav", ".flac", ".ogg", ".opus", ".aiff", ".wma"}
JUNK = {".DS_Store", ".Spotlight-V100", ".fseventsd", ".Trashes", "._.Trashes"}

# Paleta por si una carpeta no declara color. Pensada para etiquetas de vinilo
# sobre fondo negro: saturadas pero no fosforescentes.
PALETA = [0x1DB954, 0x3A6FD8, 0xC8791E, 0xB5476B, 0x2FA8A0, 0x8A63D2,
          0xD4A017, 0x5C8A3C, 0xC7522A, 0x4A7FB5]


def natural_key(p: Path):
    """'02 - x' antes de '10 - y', como espera cualquier humano."""
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", p.name)]


def parse_folder(d: Path):
    """'03 Sonidos naturales' -> (3, 'Sonidos naturales'). None si no cuadra."""
    m = re.match(r"^\s*(\d{1,2})\s*[-_. ]\s*(.+?)\s*$", d.name)
    if m:
        return int(m.group(1)), m.group(2)
    m = re.match(r"^\s*(\d{1,2})\s*$", d.name)      # solo el numero, sin nombre
    if m:
        return int(m.group(1)), f"DISCO {int(m.group(1))}"
    return None


def read_meta(d: Path):
    meta = {}
    f = d / "album.txt"
    if f.is_file():
        for line in f.read_text(encoding="utf-8", errors="replace").splitlines():
            if "=" in line and not line.strip().startswith("#"):
                k, v = line.split("=", 1)
                meta[k.strip().lower()] = v.strip()
    return meta


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
    n = 0
    for p in sorted(root.rglob("*"), key=lambda x: -len(x.parts)):
        if p.name in JUNK or p.name.startswith("._"):
            shutil.rmtree(p, ignore_errors=True) if p.is_dir() else p.unlink(missing_ok=True)
            n += 1
    print(f"  metadata de macOS borrada: {n} entradas")


def c_str(s: str) -> str:
    """La fuente Montserrat de LVGL no trae acentos: se pasa todo a ASCII."""
    tabla = str.maketrans("áéíóúÁÉÍÓÚñÑüÜ¿¡", "aeiouAEIOUnNuU??")
    return s.translate(tabla).encode("ascii", "replace").decode().replace('"', "'")


def main():
    here = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=str(here / "src"))
    ap.add_argument("--out", default=None,
                    help="punto de montaje de la tarjeta, ej. '/Volumes/NO NAME'")
    ap.add_argument("--manifest", default=str(here.parent / "firmware/VNL1/albums.h"))
    ap.add_argument("--clean-card", action="store_true")
    ap.add_argument("--wipe", action="store_true",
                    help="borra las carpetas numericas de la tarjeta antes de "
                         "escribir. Sin esto, un disco que quitaste de src/ se "
                         "queda en la tarjeta y el manifiesto deja de cuadrar "
                         "con lo que el DFPlayer ve.")
    args = ap.parse_args()

    src = Path(args.src).expanduser()
    out = Path(args.out).expanduser() if args.out else None
    if out is None:
        print("Prueba en seco: no se escribe audio. Usa --out para hacerlo real.\n")

    carpetas = []
    for d in sorted(src.iterdir() if src.is_dir() else []):
        if not d.is_dir():
            continue
        parsed = parse_folder(d)
        if not parsed:
            print(f"(ignorada: '{d.name}' no empieza con numero)")
            continue
        carpetas.append((parsed[0], parsed[1], d))
    carpetas.sort(key=lambda x: x[0])

    if not carpetas:
        print(f"No hay carpetas en {src}.")
        print("Crea una asi:  sd/src/01 FIFA/")
        return 1

    if out and args.wipe:
        borradas = []
        for p in sorted(out.iterdir()):
            if p.is_dir() and re.fullmatch(r"\d{2}", p.name):
                borradas.append(p.name)
                shutil.rmtree(p, ignore_errors=True)
        print(f"tarjeta: borradas las carpetas {', '.join(borradas) or '(ninguna)'}\n")

    manifest = []
    for num, nombre, d in carpetas:
        meta = read_meta(d)
        files = sorted([p for p in d.iterdir() if p.suffix.lower() in AUDIO_EXT],
                       key=natural_key)

        print(f"/{num:02d}  {nombre:<24} {len(files)} archivo(s)")
        if len(files) > 255:
            print("      AVISO: el DFPlayer topa en 255 pistas por carpeta")
            files = files[:255]

        durations = []
        odir = (out / f"{num:02d}") if out else None
        if odir:
            odir.mkdir(parents=True, exist_ok=True)

        # Uno por uno y en orden: el DFPlayer sigue la tabla FAT, no los nombres.
        for i, f in enumerate(files, start=1):
            if odir:
                target = odir / f"{i:03d}.mp3"
                convert(f, target)
                dur = duration_s(target)
            else:
                dur = duration_s(f)
            durations.append(dur)
            print(f"      {i:03d}.mp3  {dur//60}:{dur%60:02d}  {f.name[:44]}")

        total = sum(durations)
        if durations:
            print(f"      total {total//60} min {total%60} s")
        else:
            print(f"      (vacia: pon los audios en {d})")

        manifest.append({
            "folder": num,
            "name": c_str(meta.get("nombre", nombre)).upper(),
            "subtitle": c_str(meta.get("subtitulo", meta.get("subtitle", ""))),
            "durations": durations,
            "color": meta.get("color", f"0x{PALETA[(num - 1) % len(PALETA)]:06X}"),
        })

    if out and args.clean_card:
        print("\nLimpiando la tarjeta:")
        clean_card(out)

    # ── Manifiesto que consume el firmware ───────────────────────────────────
    L = [
        "// VNL-1 — Manifiesto de albumes.",
        "//",
        "// GENERADO por sd/prepare_sd.py a partir de las carpetas de sd/src/.",
        "// NO editar a mano: la proxima corrida del script lo sobrescribe.",
        "//",
        "// El DFPlayer no reporta duracion ni metadata: todo sale de aqui. Las",
        "// duraciones son reales, medidas con ffprobe sobre los archivos ya",
        "// convertidos. El color es el de la etiqueta del vinilo y el del anillo",
        "// de LEDs.",
        "#pragma once",
        "#include <Arduino.h>",
        "",
        "struct Album {",
        "  uint8_t         folder;         // carpeta en la microSD: 1 -> /01",
        "  const char*     name;           // ASCII: Montserrat no trae acentos",
        "  const char*     subtitle;",
        "  uint8_t         tracks;",
        "  uint16_t        seconds;        // total del album",
        "  const uint16_t* track_seconds;  // duracion de cada pista",
        "  uint32_t        color;",
        "};",
        "",
    ]
    for a in manifest:
        d = a["durations"] or [0]
        L.append(f"static const uint16_t TRACKS_{a['folder']:02d}[] = "
                 f"{{ {', '.join(str(x) for x in d)} }};")
    L += ["", "static const Album ALBUMS[] = {"]
    for a in manifest:
        L.append(f'  {{ {a["folder"]}, "{a["name"]}", "{a["subtitle"]}", '
                 f'{len(a["durations"])}, {sum(a["durations"])}, '
                 f'TRACKS_{a["folder"]:02d}, {a["color"]} }},')
    L += ["};", "",
          "static const uint8_t ALBUM_COUNT = sizeof(ALBUMS) / sizeof(ALBUMS[0]);",
          ""]

    Path(args.manifest).write_text("\n".join(L))
    print(f"\nmanifiesto escrito: {args.manifest}")
    print(f"{len(manifest)} disco(s). Recompila y flashea para verlos en pantalla.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
