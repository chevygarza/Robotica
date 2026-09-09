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

ESPECIALES = {}

ORIGEN   = "_origen"
ASSETS   = "assets"      # caratulas de los discos especiales (alarma, ajustes)
# La caratula se genera al tamano del DISCO, no de la etiqueta: en la
# biblioteca ocupa el disco completo. Debe coincidir con VINYL_DISC_D.
LABEL_PX = 336

# Paleta de respaldo para carpetas sin caratula ni color declarado.
PALETA = [0x1DB954, 0x3A6FD8, 0xC8791E, 0xB5476B, 0x2FA8A0, 0x8A63D2,
          0xD4A017, 0x5C8A3C, 0xC7522A, 0x4A7FB5]

FQBN = ("esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,"
        "PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc")

# Particion propia: 8MB para la aplicacion en vez de los 3MB de huge_app. Con
# 220KB por caratula, 3MB topaban en nueve discos. El archivo vive en
# <core esp32>/tools/partitions/vnl1_8M.csv
BUILD_PROPS = ["--build-property", "build.partitions=vnl1_8M",
               "--build-property", "upload.maximum_size=8388608"]


# ── Utilidades ───────────────────────────────────────────────────────────────
def es_util(p: Path) -> bool:
    """macOS crea gemelos '._archivo.mp3' al copiar a FAT32. Terminan en .mp3
    pero no son audio: si se cuelan, ffmpeg truena y ademas se duplicarian las
    pistas. Se filtran ANTES de listar, no despues."""
    return not p.name.startswith(".")


def limpiar_basura(root: Path) -> int:
    """Borra la metadata invisible de macOS, tolerando lo que no se deje.

    Algunos archivos de sistema (._.Spotlight-V100 y compania) estan
    protegidos y no se pueden eliminar ni con permisos. No importan: el
    DFPlayer los ignora igual. Lo que no se puede borrar se salta, en vez de
    tumbar todo el proceso por un archivo irrelevante.
    """
    n = 0
    for p in sorted(root.rglob("*"), key=lambda x: -len(x.parts)):
        if not (p.name in JUNK or p.name.startswith("._")):
            continue
        try:
            if p.is_dir():
                shutil.rmtree(p, ignore_errors=True)
            else:
                p.unlink()
            n += 1
        except (PermissionError, OSError):
            pass          # protegido por el sistema: se queda y no estorba
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


def firma(files):
    """Huella de lo que hay en la carpeta: nombres y tamanos, en orden."""
    return "\n".join(f"{f.name}\t{f.stat().st_size}" for f in files)


def ya_convertido(d: Path, odir: Path, files) -> bool:
    """True si las pistas convertidas siguen correspondiendo al origen.

    Convertir audio es lo lento del proceso, y casi siempre lo unico que
    cambia es una caratula. Se guarda una huella de los archivos fuente y, si
    coincide y los destinos existen, se salta la conversion.
    """
    marca = d / ".sync"
    if not marca.is_file() or marca.read_text() != firma(files):
        return False
    return all((odir / f"{i:03d}.mp3").is_file() for i in range(1, len(files) + 1))


def duration_s(path: Path) -> int:
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "json", str(path)], capture_output=True, text=True, check=True).stdout
    return int(round(float(json.loads(out)["format"]["duration"])))


# El DFPlayer no entrega metadata: si el nombre de la cancion no sale de aqui,
# no sale de ningun lado. Se leen del archivo ORIGEN y no del convertido,
# porque el origen es el que trae los tags de donde sea que vino la musica.
NUM_AL_INICIO = re.compile(r"^\s*\d{1,3}\s*[-_.)]?\s+")

# Casi toda esta musica viene de YouTube, y los tags traen el ruido del titulo
# del video. Se quita solo el parentesis o corchete que contenga una de estas
# palabras: "(Latino)" o "(Spanish Version)" dicen algo y se quedan.
RUIDO = re.compile(
    r"[\(\[][^)\]]*(official|oficial|lyric|letra|audio|video|remaster|"
    r"\bhd\b|\bhq\b|\b4k\b|visualizer|explicit|original album version|"
    r"full album|sub espanol|subtitulado)[^)\]]*[\)\]]", re.I)
EXT_PEGADA = re.compile(r"\.(wmv|mp4|mkv|avi|m4a|mp3|webm)\b", re.I)
# El identificador que yt-dlp pega al final: "[lMRziQRmYLI]". Son once
# caracteres y siempre al final, asi que se puede quitar sin riesgo.
ID_YOUTUBE = re.compile(r"\s*\[[A-Za-z0-9_-]{11}\]\s*$")
# Titulos que el tag trae pero que no dicen nada: vale mas el nombre del archivo.
VACIOS = {"untitled", "unknown", "track", "audio track", "sin titulo", ""}


def limpiar_titulo(t: str, artista: str) -> str:
    t = RUIDO.sub("", t)
    t = ID_YOUTUBE.sub("", t)
    t = EXT_PEGADA.sub("", t)
    # "Blur - Song 2" con artista "Blur" -> "Song 2". Solo si de verdad coincide:
    # en "C418 - Beginning" con artista "SMORT" no hay nada que quitar.
    if artista:
        a = re.escape(artista.strip())
        t = re.sub(rf"^\s*{a}\s*[-–|]\s*", "", t, flags=re.I)
    t = re.sub(r"\s+", " ", t)
    return t.strip(" -–|.,")


def corta(s: str, n: int) -> str:
    """Recorta sin partir palabras a la mitad."""
    if len(s) <= n:
        return s
    corte = s[:n].rsplit(" ", 1)[0]
    return (corte or s[:n]).rstrip(" -–|.,")


def tags(path: Path):
    """Titulo y artista crudos del archivo. Vacios si no hay tags."""
    try:
        out = subprocess.run(
            ["ffprobe", "-v", "error", "-show_entries", "format_tags=title,artist",
             "-of", "json", str(path)],
            capture_output=True, text=True, check=True).stdout
        t = json.loads(out).get("format", {}).get("tags", {}) or {}
    except Exception:
        t = {}
    # Las llaves llegan con mayusculas distintas segun quien escribio el tag.
    baja = {k.lower(): (v or "").strip() for k, v in t.items()}
    return baja.get("title", ""), baja.get("artist", "")


def pista_meta(f: Path, i: int, meta: dict):
    """(titulo, artista) ya limpios. album.txt manda sobre los tags.

    En album.txt se puede poner:
        artista = C418          <- para todas las pistas del disco
        1 = Beginning           <- titulo de una pista concreta
        1.artista = Blur        <- artista de una pista concreta
    Es la salida para los tags que vienen del canal de YouTube y no del autor:
    ningun script puede adivinar que "SMORT" no es quien compuso Minecraft.
    """
    titulo, artista = tags(f)
    artista = meta.get(f"{i}.artista", meta.get("artista", artista))
    if titulo.strip().lower() in VACIOS:
        titulo = ""
    if not titulo:
        # "03 - Cancion.mp3" -> "Cancion". El numero ya lo dice el aparato.
        titulo = NUM_AL_INICIO.sub("", f.stem).strip() or f.stem
    titulo = meta.get(str(i), limpiar_titulo(titulo, artista))
    return corta(c_str(titulo), 44), corta(c_str(artista), 30)


def convert(src: Path, dst: Path):
    # 192kbps CBR 44.1kHz: dentro de lo que el DFPlayer digiere con margen, y
    # sin VBR (algunos lotes se atragantan al buscar dentro de un VBR).
    subprocess.run(
        ["ffmpeg", "-nostdin", "-loglevel", "error", "-y", "-i", str(src),
         "-map", "0:a:0", "-c:a", "libmp3lame", "-b:a", "192k",
         "-ar", "44100", "-ac", "2", "-write_xing", "0", str(dst)], check=True)


# ── Caratulas ────────────────────────────────────────────────────────────────
def paleta_anillo(im, n=8):
    """Un color por LED, tomado del sector de la caratula que le queda detras.

    Se promedia solo el anillo exterior de la imagen (del 45% al 95% del radio)
    porque es la zona que el LED tiene enfrente. Despues se le sube saturacion
    y luminosidad: un color fiel puede ser muy oscuro, y un LED oscuro no
    alumbra — es lo que le paso a la primera portada de Zelda.
    """
    import colorsys, math
    W = im.size[0]
    px = im.load()
    c = W / 2.0
    acc = [[0, 0, 0, 0] for _ in range(n)]
    for y in range(0, W, 2):
        for x in range(0, W, 2):
            dx, dy = x - c, y - c
            r = math.hypot(dx, dy)
            if r < c * 0.45 or r > c * 0.95:
                continue
            # El sector 0 arranca arriba y avanza en sentido horario.
            ang = (math.degrees(math.atan2(dy, dx)) + 90.0) % 360.0
            k = int(ang * n / 360.0) % n
            pr, pg, pb = px[x, y]
            acc[k][0] += pr; acc[k][1] += pg; acc[k][2] += pb; acc[k][3] += 1

    out = []
    for r_, g_, b_, cnt in acc:
        if cnt == 0:
            out.append(0xFFFFFF); continue
        r_, g_, b_ = r_ / cnt / 255, g_ / cnt / 255, b_ / cnt / 255
        h, sat, val = colorsys.rgb_to_hsv(r_, g_, b_)
        sat = max(sat, 0.60)
        val = max(val, 0.80)
        r_, g_, b_ = colorsys.hsv_to_rgb(h, sat, val)
        out.append((int(r_ * 255) << 16) | (int(g_ * 255) << 8) | int(b_ * 255))
    return out


def process_cover(img_path: Path):
    """Devuelve (color_dominante, pixeles_rgb565, paleta_del_anillo)."""
    try:
        from PIL import Image
    except ImportError:
        print("      (sin Pillow: no puedo procesar la caratula)")
        return None, None, None

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

    raw = im.tobytes()
    rgb565 = [((raw[i] & 0xF8) << 8) | ((raw[i + 1] & 0xFC) << 3) | (raw[i + 2] >> 3)
              for i in range(0, len(raw), 3)]
    return ((mejor[0] << 16) | (mejor[1] << 8) | mejor[2], rgb565,
            paleta_anillo(im))


# ── Salidas ──────────────────────────────────────────────────────────────────
def write_albums_h(path: Path, manifest):
    L = ["// VNL-1 — Manifiesto de albumes.",
         "//",
         "// GENERADO por sd/prepare_sd.py desde la carpeta _origen de la microSD.",
         "// NO editar a mano: la proxima corrida lo sobrescribe.",
         "//",
         "// El DFPlayer no reporta duracion ni metadata: todo sale de aqui. Las",
         "// duraciones y los nombres de pista son reales, sacados con ffprobe de",
         "// los archivos originales. El color es el dominante",
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
         "  const char* const* track_names; // titulo de cada pista",
         "  const char* const* track_artists;",
         "  uint32_t        color;",
         "  const uint16_t* cover;          // nullptr = etiqueta dibujada",
         "  const uint32_t* anillo;         // 8 colores, uno por LED",
         "};",
         ""]
    for a in manifest:
        d = a["durations"] or [0]
        L.append(f"static const uint16_t TRACKS_{a['folder']:02d}[] = "
                 f"{{ {', '.join(str(x) for x in d)} }};")
    L.append("")
    for a in manifest:
        n = a.get("nombres") or [""]
        ar = a.get("artistas") or [""]
        L.append(f"static const char* const NOMBRES_{a['folder']:02d}[] = "
                 + "{ " + ", ".join(f'"{x}"' for x in n) + " };")
        L.append(f"static const char* const ARTISTAS_{a['folder']:02d}[] = "
                 + "{ " + ", ".join(f'"{x}"' for x in ar) + " };")
    L.append("")
    for a in manifest:
        if a.get("anillo"):
            L.append(f"static const uint32_t RING_{a['folder']:02d}[8] = {{ "
                     + ", ".join(f"0x{c:06X}" for c in a["anillo"]) + " };")
    L += ["", "static const Album ALBUMS[] = {"]
    for a in manifest:
        cover = f"COVER_{a['folder']:02d}" if a["cover"] else "nullptr"
        L.append(f'  {{ {a["folder"]}, "{a["name"]}", "{a["subtitle"]}", '
                 f'{len(a["durations"])}, {sum(a["durations"])}, '
                 f'TRACKS_{a["folder"]:02d}, NOMBRES_{a["folder"]:02d}, '
                 f'ARTISTAS_{a["folder"]:02d}, 0x{a["color"]:06X}, {cover}, '
                 f'{("RING_%02d" % a["folder"]) if a.get("anillo") else "nullptr"} }},')
    L += ["};", "",
          "static const uint8_t ALBUM_COUNT = sizeof(ALBUMS) / sizeof(ALBUMS[0]);",
          ""]
    path.write_text("\n".join(L))


def cover_especial(nombre: str, here: Path):
    """Caratula de un disco que no vive en la microSD (alarma, ajustes).

    Estos dos no son albumes: son parte del firmware. Su imagen vive en
    sd/assets/ y se procesa igual que cualquier portada.
    """
    d = here / ASSETS
    if not d.is_dir():
        return None, None, None
    for p in sorted(d.iterdir()):
        if p.stem.lower() == nombre and p.suffix.lower() in IMG_EXT and es_util(p):
            print(f"caratula especial: {p.name} -> {nombre}")
            return process_cover(p)
    return None, None, None


def write_especiales(fw: Path, here: Path):
    """Caratulas de Alarma y Ajustes, en su propio par de archivos.

    Separadas de covers.h a proposito: no dependen de la microSD, asi que se
    pueden regenerar sin tenerla conectada. Alarma y Ajustes son parte del
    firmware, no albumes.
    """
    H = ["// VNL-1 — Caratulas de los discos especiales. GENERADO, no editar.",
         "#pragma once", "#include <Arduino.h>", ""]
    C = ["// VNL-1 — Datos de las caratulas especiales. GENERADO, no editar.",
         '#include "especiales.h"', ""]
    for nombre in ("alarma", "ajustes"):
        col, px, anillo = cover_especial(nombre, here)
        MAY = nombre.upper()
        if not px:
            H.append(f"#define COVER_{MAY}_OK 0")
            continue
        H.append(f"#define COVER_{MAY}_OK 1")
        H.append(f"#define COLOR_{MAY}_AUTO 0x{col:06X}")
        H.append(f"extern const uint16_t COVER_{MAY}[];")
        H.append(f"static const uint32_t RING_{MAY}[8] = {{ "
                 + ", ".join(f"0x{c:06X}" for c in anillo) + " };")
        C.append(f"const uint16_t COVER_{MAY}[] = {{")
        for i in range(0, len(px), 16):
            C.append("  " + ",".join(f"0x{v:04X}" for v in px[i:i + 16]) + ",")
        C += ["};", ""]
    H.append("")
    (fw / "especiales.h").write_text("\n".join(H))
    (fw / "especiales.cpp").write_text("\n".join(C))


def write_covers_h(path: Path, manifest):
    """Escribe covers.h (declaraciones) y covers.cpp (datos, una sola vez).

    Separadas a proposito: un arreglo "static" en una cabecera se duplica en
    cada .cpp que la incluye. Con dos archivos incluyendola, cada caratura
    ocupaba el doble de flash.
    """
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
    C = ['// VNL-1 — Definicion UNICA de las caratulas. GENERADO, no editar.',
         '#include "covers.h"', '']

    for a in manifest:
        if not a["cover"]:
            continue
        L.append(f"extern const uint16_t COVER_{a['folder']:02d}[];")
        C.append(f"const uint16_t COVER_{a['folder']:02d}[] = {{")
        datos = a["cover"]
        for i in range(0, len(datos), 16):
            C.append("  " + ",".join(f"0x{v:04X}" for v in datos[i:i + 16]) + ",")
        C += ["};", ""]
    L.append("")
    path.write_text("\n".join(L))
    path.with_suffix(".cpp").write_text("\n".join(C))


def flashear(fw: Path):
    ports = glob.glob("/dev/cu.usbmodem*")
    if not ports:
        print("\nLa perilla no esta conectada: no puedo flashear.")
        print("Conectala por USB y vuelve a correr esto.")
        return
    print(f"\nCompilando y flasheando en {ports[0]} ...\n")
    r = subprocess.run(["arduino-cli", "compile", "--upload", "-p", ports[0],
                        "--fqbn", FQBN] + BUILD_PROPS + [str(fw)])
    print("\nListo." if r.returncode == 0 else "\nFallo el flasheo.")


# ── Principal ────────────────────────────────────────────────────────────────
def main():
    here = Path(__file__).resolve().parent
    fw = here.parent / "firmware/VNL1"

    ap = argparse.ArgumentParser()
    ap.add_argument("--card", default=None, help="ruta de la tarjeta")
    ap.add_argument("--flash", action="store_true", help="compila y flashea al terminar")
    ap.add_argument("--dry-run", action="store_true", help="no escribe nada")
    args = ap.parse_args()

    # Las caratulas de Alarma y Ajustes viven en el Mac, no en la tarjeta, asi
    # que se actualizan aunque no haya microSD conectada.
    if not args.dry_run:
        write_especiales(fw, here)

    card = Path(args.card) if args.card else find_card()
    if not card or not card.is_dir():
        print("Sin microSD: solo se actualizaron Alarma y Ajustes.")
        print("Conectala si tambien quieres sincronizar la musica.\n")
        if args.flash and not args.dry_run:
            flashear(fw)
        return 0
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

    # Solo se borran las carpetas numericas que ya NO tienen origen: si quitaste
    # un disco tiene que desaparecer de la tarjeta o el manifiesto deja de
    # cuadrar. Las que siguen vivas se dejan, para no obligar a reconvertir.
    vivos = {f"{n:02d}" for n, _, _ in carpetas}
    if not args.dry_run:
        for p in sorted(card.iterdir()):
            if p.is_dir() and re.fullmatch(r"\d{2}", p.name) and p.name not in vivos:
                print(f"tarjeta: sobra la carpeta {p.name}, la quito")
                shutil.rmtree(p, ignore_errors=True)

    manifest = []
    for num, nombre, d in carpetas:
        meta = read_meta(d)
        files = sorted([p for p in d.iterdir()
                        if p.suffix.lower() in AUDIO_EXT and es_util(p)],
                       key=natural_key)
        print(f"/{num:02d}  {nombre:<22} {len(files)} cancion(es)")

        durations = []
        nombres, artistas = [], []
        odir = card / f"{num:02d}"
        files = files[:255]
        if not args.dry_run:
            odir.mkdir(parents=True, exist_ok=True)

        saltar = (not args.dry_run) and ya_convertido(d, odir, files)
        if saltar:
            for i, f in enumerate(files, start=1):
                durations.append(duration_s(odir / f"{i:03d}.mp3"))
                t, ar = pista_meta(f, i, meta)
                nombres.append(t); artistas.append(ar)
            print(f"      musica sin cambios, no se reconvierte")
        else:
            # Uno por uno y en orden: el DFPlayer sigue la tabla FAT, no los
            # nombres de archivo.
            for i, f in enumerate(files, start=1):
                if args.dry_run:
                    dur = duration_s(f)
                else:
                    target = odir / f"{i:03d}.mp3"
                    convert(f, target)
                    dur = duration_s(target)
                durations.append(dur)
                t, ar = pista_meta(f, i, meta)
                nombres.append(t); artistas.append(ar)
                print(f"      {i:03d}.mp3  {dur//60}:{dur%60:02d}  {t[:42]}")
            if not args.dry_run and files:
                (d / ".sync").write_text(firma(files))

        if durations:
            tot = sum(durations)
            print(f"      total {tot//60} min {tot%60} s")
        else:
            print("      (sin musica todavia)")

        color, cover, anillo = None, None, None
        # Prioriza el archivo llamado "cover": en las carpetas suelen quedar
        # miniaturas que se bajaron con la musica, y por orden alfabetico una
        # de esas le ganaria a la portada de verdad.
        imgs = sorted([p for p in d.iterdir()
                       if p.suffix.lower() in IMG_EXT and es_util(p)],
                      key=lambda p: (p.stem.lower() != "cover", p.name.lower()))
        if imgs:
            color, cover, anillo = process_cover(imgs[0])
            if color is not None:
                print(f"      caratula {imgs[0].name[:26]} -> 0x{color:06X}")
        if "color" in meta:
            color = int(meta["color"], 16)
        if color is None:
            color = PALETA[(num - 1) % len(PALETA)]

        manifest.append({
            "folder": num,
            "nombres": nombres,
            "artistas": artistas,
            "name": c_str(meta.get("nombre", nombre)).upper(),
            "subtitle": c_str(meta.get("subtitulo", meta.get("subtitle", ""))),
            "durations": durations,
            "color": color,
            "cover": cover,
            "anillo": anillo,
        })

    if not args.dry_run:
        print(f"\nmetadata de macOS borrada de la tarjeta: {limpiar_basura(card)} entradas")

        write_albums_h(fw / "albums.h", manifest)
        write_covers_h(fw / "covers.h", manifest)
        print(f"manifiesto y caratulas escritos en {fw}")

    print(f"\n{len(manifest)} disco(s) en la tarjeta.")

    if args.flash and not args.dry_run:
        flashear(fw)
    return 0


if __name__ == "__main__":
    sys.exit(main())
