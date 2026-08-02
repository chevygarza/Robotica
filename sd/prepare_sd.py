#!/usr/bin/env python3
"""
VNL-1 — Sincroniza la microSD y regenera el manifiesto y las caratulas.

EL ORIGEN VIVE EN LA PROPIA TARJETA, en la carpeta _origen:

    /Volumes/<TARJETA>/_origen/01 FIFA/
                              /02 Zelda/
                              /03 Minecraft/  (musica + cover.jpg opcional)

Ahi arrastras la musica con el nombre que sea. El DFPlayer ignora las carpetas
que no son numericas, asi que _origen le es invisible: convive con /01, /02
sin estorbarle.

De cada carpeta salen tres cosas:
  · /NN/001.mp3 ...     lo unico que el DFPlayer sabe leer
  · albums.h            nombres, duraciones reales y colores
  · covers.h            la caratula convertida a pixeles para la pantalla

Uso normal: doble clic en "Actualizar VinilOS.command". A mano:

    python3 prepare_sd.py                 # detecta la tarjeta y sincroniza
    python3 prepare_sd.py --flash         # ademas compila y flashea la perilla
    python3 prepare_sd.py --dry-run       # solo mira, no escribe

Dos trampas del DFPlayer que este script cubre:

1) NO ordena por nombre de archivo, sino por el orden de la tabla FAT. Por eso
   los archivos se copian uno por uno, en secuencia.
2) macOS escribe metadata invisible (.DS_Store, ._001.mp3, .Spotlight-V100) y
   el modulo la cuenta como pistas: te toca silencio donde deberia ir musica.
   Se borra al final de cada corrida.
"""
import argparse, glob, json, re, shutil, subprocess, sys
from pathlib import Path

AUDIO_EXT = {".mp3", ".m4a", ".aac", ".wav", ".flac", ".ogg", ".opus", ".aiff", ".wma"}
IMG_EXT   = {".jpg", ".jpeg", ".png", ".webp", ".bmp"}
JUNK      = {".DS_Store", ".Spotlight-V100", ".fseventsd", ".Trashes", "._.Trashes"}

ORIGEN   = "_origen"
LABEL_PX = 172           # debe coincidir con VINYL_LABEL_D en vinyl.h

# Paleta de respaldo para carpetas sin caratula ni color declarado.
PALETA = [0x1DB954, 0x3A6FD8, 0xC8791E, 0xB5476B, 0x2FA8A0, 0x8A63D2,
          0xD4A017, 0x5C8A3C, 0xC7522A, 0x4A7FB5]

FQBN = ("esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,"
        "PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc")


# ── Utilidades ───────────────────────────────────────────────────────────────
def es_util(p: Path) -> bool:
    """macOS crea gemelos '._archivo.mp3' al copiar a FAT32. Terminan en .mp3
    pero no son audio: si se cuelan, ffmpeg truena y ademas se duplicarian las
    pistas. Se filtran ANTES de listar, no despues."""
    return not p.name.startswith(".")


def limpiar_basura(root: Path) -> int:
    n = 0
    for p in sorted(root.rglob("*"), key=lambda x: -len(x.parts)):
        if p.name in JUNK or p.name.startswith("._"):
            shutil.rmtree(p, ignore_errors=True) if p.is_dir() else p.unlink(missing_ok=True)
            n += 1
    return n


def natural_key(p: Path):
    """'02 - x' antes de '10 - y', como espera cualquier humano."""
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", p.name)]


def find_card():
    """La tarjeta es el volumen que tenga una carpeta _origen."""
    vols = Path("/Volumes")
    if not vols.is_dir():
        return None
    otros = [v for v in vols.iterdir() if v.is_dir() and v.name != "Macintosh HD"]
    con_origen = [v for v in otros if (v / ORIGEN).is_dir()]
    if con_origen:
        return con_origen[0]
    return otros[0] if len(otros) == 1 else None


def parse_folder(d: Path):
    """'03 Minecraft' -> (3, 'Minecraft'). None si no empieza con numero."""
    m = re.match(r"^\s*(\d{1,2})\s*[-_. ]\s*(.+?)\s*$", d.name)
    if m:
        return int(m.group(1)), m.group(2)
    m = re.match(r"^\s*(\d{1,2})\s*$", d.name)
    return (int(m.group(1)), f"DISCO {int(m.group(1))}") if m else None


def read_meta(d: Path):
    meta = {}
    f = d / "album.txt"
    if f.is_file():
        for line in f.read_text(encoding="utf-8", errors="replace").splitlines():
            if "=" in line and not line.strip().startswith("#"):
                k, v = line.split("=", 1)
                meta[k.strip().lower()] = v.strip()
    return meta


def c_str(s: str) -> str:
    """La fuente Montserrat de LVGL no trae acentos: todo a ASCII."""
    t = str.maketrans("aeiouAEIOUnNuU", "aeiouAEIOUnNuU")
    s = (s.replace("á", "a").replace("é", "e").replace("í", "i")
          .replace("ó", "o").replace("ú", "u").replace("ñ", "n")
          .replace("Á", "A").replace("É", "E").replace("Í", "I")
          .replace("Ó", "O").replace("Ú", "U").replace("Ñ", "N")
          .replace("ü", "u").replace("Ü", "U"))
    return s.translate(t).encode("ascii", "replace").decode().replace('"', "'")


def duration_s(path: Path) -> int:
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "json", str(path)], capture_output=True, text=True, check=True).stdout
    return int(round(float(json.loads(out)["format"]["duration"])))


def convert(src: Path, dst: Path):
    # 192kbps CBR 44.1kHz: dentro de lo que el DFPlayer digiere con margen, y
    # sin VBR (algunos lotes se atragantan al buscar dentro de un VBR).
    subprocess.run(
        ["ffmpeg", "-nostdin", "-loglevel", "error", "-y", "-i", str(src),
         "-map", "0:a:0", "-c:a", "libmp3lame", "-b:a", "192k",
         "-ar", "44100", "-ac", "2", "-write_xing", "0", str(dst)], check=True)


# ── Caratulas ────────────────────────────────────────────────────────────────
def process_cover(img_path: Path):
    """Devuelve (color_dominante, lista_rgb565) o (None, None)."""
    try:
        from PIL import Image
    except ImportError:
        print("      (sin Pillow: no puedo procesar la caratula)")
        return None, None

    im = Image.open(img_path)
    # Un PNG con transparencia no se puede convertir a RGB de golpe: eso tira
    # el canal alfa y deja a la vista lo que hubiera debajo, que suele ser
    # basura. Hay que componerlo sobre un fondo primero. Va sobre negro, que
    # es lo que mejor le queda a la etiqueta de un vinilo.
    if im.mode in ("RGBA", "LA", "P"):
        im = im.convert("RGBA")
        fondo = Image.new("RGBA", im.size, (0, 0, 0, 255))
        im = Image.alpha_composite(fondo, im)
    im = im.convert("RGB")

    # Recorte cuadrado centrado: la etiqueta es un circulo, cualquier otra
    # proporcion se deformaria al ajustarla.
    w, h = im.size
    lado = min(w, h)
    im = im.crop(((w - lado) // 2, (h - lado) // 2,
                  (w + lado) // 2, (h + lado) // 2))
    im = im.resize((LABEL_PX, LABEL_PX), Image.LANCZOS)

    # Color dominante: se agrupa la imagen en 8 tonos y se elige el que mejor
    # combina frecuencia y saturacion. Un promedio simple sale lodoso: mezcla
    # todo y devuelve un gris pardo que no representa nada.
    q = im.quantize(colors=8, method=Image.MEDIANCUT)
    pal = q.getpalette()
    mejor, mejor_score = None, -1.0
    total = LABEL_PX * LABEL_PX
    for count, idx in q.getcolors():
        r, g, b = pal[idx * 3:idx * 3 + 3]
        mx, mn = max(r, g, b), min(r, g, b)
        sat = 0.0 if mx == 0 else (mx - mn) / mx
        lum = (r * 299 + g * 587 + b * 114) / 1000
        if lum < 28 or lum > 232:          # ni negros ni blancos: no dan color
            continue
        score = (count / total) * (0.35 + sat)
        if score > mejor_score:
            mejor_score, mejor = score, (r, g, b)
    if mejor is None:
        mejor = im.resize((1, 1), Image.LANCZOS).getpixel((0, 0))

    rgb565 = [((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
              for r, g, b in im.getdata()]
    return (mejor[0] << 16) | (mejor[1] << 8) | mejor[2], rgb565


# ── Salidas ──────────────────────────────────────────────────────────────────
def write_albums_h(path: Path, manifest):
    L = ["// VNL-1 — Manifiesto de albumes.",
         "//",
         "// GENERADO por sd/prepare_sd.py desde la carpeta _origen de la microSD.",
         "// NO editar a mano: la proxima corrida lo sobrescribe.",
         "//",
         "// El DFPlayer no reporta duracion ni metadata: todo sale de aqui. Las",
         "// duraciones son reales, medidas con ffprobe. El color es el dominante",
         "// de la caratula, y se usa en la etiqueta y en el anillo de LEDs.",
         "#pragma once",
         "#include <Arduino.h>",
         '#include "covers.h"',
         "",
         "struct Album {",
         "  uint8_t         folder;         // carpeta en la microSD: 1 -> /01",
         "  const char*     name;           // ASCII: Montserrat no trae acentos",
         "  const char*     subtitle;",
         "  uint8_t         tracks;",
         "  uint16_t        seconds;        // total del album",
         "  const uint16_t* track_seconds;  // duracion de cada pista",
         "  uint32_t        color;",
         "  const uint16_t* cover;          // nullptr = etiqueta dibujada",
         "};",
         ""]
    for a in manifest:
        d = a["durations"] or [0]
        L.append(f"static const uint16_t TRACKS_{a['folder']:02d}[] = "
                 f"{{ {', '.join(str(x) for x in d)} }};")
    L += ["", "static const Album ALBUMS[] = {"]
    for a in manifest:
        cover = f"COVER_{a['folder']:02d}" if a["cover"] else "nullptr"
        L.append(f'  {{ {a["folder"]}, "{a["name"]}", "{a["subtitle"]}", '
                 f'{len(a["durations"])}, {sum(a["durations"])}, '
                 f'TRACKS_{a["folder"]:02d}, 0x{a["color"]:06X}, {cover} }},')
    L += ["};", "",
          "static const uint8_t ALBUM_COUNT = sizeof(ALBUMS) / sizeof(ALBUMS[0]);",
          ""]
    path.write_text("\n".join(L))


def write_covers_h(path: Path, manifest):
    L = ["// VNL-1 — Caratulas convertidas a pixeles.",
         "//",
         "// GENERADO por sd/prepare_sd.py. NO editar a mano.",
         "// Cada caratula son 172x172 en RGB565: unos 59KB de flash. El recorte",
         "// circular lo hace el firmware al dibujarla sobre la etiqueta.",
         "#pragma once",
         "#include <Arduino.h>",
         "",
         f"#define COVER_PX {LABEL_PX}",
         ""]
    for a in manifest:
        if not a["cover"]:
            continue
        L.append(f"static const uint16_t COVER_{a['folder']:02d}[] = {{")
        datos = a["cover"]
        for i in range(0, len(datos), 16):
            L.append("  " + ",".join(f"0x{v:04X}" for v in datos[i:i + 16]) + ",")
        L += ["};", ""]
    path.write_text("\n".join(L))


# ── Principal ────────────────────────────────────────────────────────────────
def main():
    here = Path(__file__).resolve().parent
    fw = here.parent / "firmware/VNL1"

    ap = argparse.ArgumentParser()
    ap.add_argument("--card", default=None, help="ruta de la tarjeta")
    ap.add_argument("--flash", action="store_true", help="compila y flashea al terminar")
    ap.add_argument("--dry-run", action="store_true", help="no escribe nada")
    args = ap.parse_args()

    card = Path(args.card) if args.card else find_card()
    if not card or not card.is_dir():
        print("No encuentro la microSD.")
        print("Conectala al Mac y vuelve a correr esto.")
        return 1
    print(f"Tarjeta: {card}\n")

    src = card / ORIGEN
    if not src.is_dir():
        src.mkdir(parents=True, exist_ok=True)
        print(f"Cree la carpeta {ORIGEN}. Pon ahi tus discos, asi:")
        print(f"   {ORIGEN}/01 FIFA/     (musica + cover.jpg opcional)")
        return 0

    if not args.dry_run:
        n = limpiar_basura(src)
        if n:
            print(f"metadata de macOS borrada del origen: {n} entradas\n")

    carpetas = []
    for d in sorted(src.iterdir()):
        if not d.is_dir() or d.name.startswith("."):
            continue
        p = parse_folder(d)
        if p:
            carpetas.append((p[0], p[1], d))
        else:
            print(f"(ignorada: '{d.name}' no empieza con numero)")
    carpetas.sort(key=lambda x: x[0])

    if not carpetas:
        print(f"No hay discos en {ORIGEN}. Crea uno asi:  {ORIGEN}/01 FIFA/")
        return 0

    # Las carpetas numericas se rehacen desde cero: si quitaste un disco del
    # origen tiene que desaparecer tambien de la tarjeta, o el manifiesto deja
    # de cuadrar con lo que el DFPlayer ve.
    if not args.dry_run:
        for p in sorted(card.iterdir()):
            if p.is_dir() and re.fullmatch(r"\d{2}", p.name):
                shutil.rmtree(p, ignore_errors=True)

    manifest = []
    for num, nombre, d in carpetas:
        meta = read_meta(d)
        files = sorted([p for p in d.iterdir()
                        if p.suffix.lower() in AUDIO_EXT and es_util(p)],
                       key=natural_key)
        print(f"/{num:02d}  {nombre:<22} {len(files)} cancion(es)")

        durations = []
        odir = card / f"{num:02d}"
        if not args.dry_run:
            odir.mkdir(parents=True, exist_ok=True)

        # Uno por uno y en orden: el DFPlayer sigue la tabla FAT, no los nombres.
        for i, f in enumerate(files[:255], start=1):
            if args.dry_run:
                dur = duration_s(f)
            else:
                target = odir / f"{i:03d}.mp3"
                convert(f, target)
                dur = duration_s(target)
            durations.append(dur)
            print(f"      {i:03d}.mp3  {dur//60}:{dur%60:02d}  {f.name[:42]}")

        if durations:
            tot = sum(durations)
            print(f"      total {tot//60} min {tot%60} s")
        else:
            print("      (sin musica todavia)")

        color, cover = None, None
        imgs = [p for p in d.iterdir()
                if p.suffix.lower() in IMG_EXT and es_util(p)]
        if imgs:
            color, cover = process_cover(sorted(imgs)[0])
            if color is not None:
                print(f"      caratula {sorted(imgs)[0].name[:26]} -> 0x{color:06X}")
        if "color" in meta:
            color = int(meta["color"], 16)
        if color is None:
            color = PALETA[(num - 1) % len(PALETA)]

        manifest.append({
            "folder": num,
            "name": c_str(meta.get("nombre", nombre)).upper(),
            "subtitle": c_str(meta.get("subtitulo", meta.get("subtitle", ""))),
            "durations": durations,
            "color": color,
            "cover": cover,
        })

    if not args.dry_run:
        print(f"\nmetadata de macOS borrada de la tarjeta: {limpiar_basura(card)} entradas")

        write_albums_h(fw / "albums.h", manifest)
        write_covers_h(fw / "covers.h", manifest)
        print(f"manifiesto y caratulas escritos en {fw}")

    print(f"\n{len(manifest)} disco(s) en la tarjeta.")

    if args.flash and not args.dry_run:
        ports = glob.glob("/dev/cu.usbmodem*")
        if not ports:
            print("\nLa perilla no esta conectada: no puedo flashear.")
            print("Conectala por USB y vuelve a correr esto.")
            return 0
        print(f"\nCompilando y flasheando en {ports[0]} ...\n")
        r = subprocess.run(["arduino-cli", "compile", "--upload", "-p", ports[0],
                            "--fqbn", FQBN, str(fw)])
        print("\nListo." if r.returncode == 0 else "\nFallo el flasheo.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
