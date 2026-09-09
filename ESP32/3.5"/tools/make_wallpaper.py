#!/usr/bin/env python3
"""Genera el wallpaper de TactOS: 480x320, RGB565 big-endian, como lv_img_dsc_t.

Un solo fondo, sobrio: negro con un resplandor ambar abajo a la izquierda, como
una lampara en la mesa. Se tramea con ruido antes de bajar a 16 bits para que
el degradado no se vea en bandas.

Uso:  .venv/bin/python make_wallpaper.py [--preview salida.png]
Escribe firmware/TAC1/wallpaper.c
"""
import math, random, struct, sys, pathlib

W, H = 480, 320
AMBAR = (0xFF, 0x7A, 0x10)
OUT   = pathlib.Path(__file__).resolve().parent.parent / "firmware" / "TAC1" / "wallpaper.c"

def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)

def pixel(x, y):
    # Base: carbon muy oscuro que se apaga hacia abajo.
    v = 1 - y / (H - 1)
    base = (12 + 10 * v, 14 + 12 * v, 18 + 16 * v)
    # Resplandor ambar, centro fuera del cuadro abajo a la izquierda.
    d = math.hypot((x - 40) / 1.15, (y - 360))
    g = smooth(1 - d / 420) ** 1.8 * 0.55
    # Un segundo halo, mas frio y mas tenue, arriba a la derecha: da profundidad
    # sin meter un color nuevo (es blanco al 6%).
    d2 = math.hypot((x - 470), (y + 60) / 1.3)
    h = smooth(1 - d2 / 380) ** 2 * 0.06
    r = base[0] + AMBAR[0] * g + 255 * h
    gg = base[1] + AMBAR[1] * g + 255 * h
    b = base[2] + AMBAR[2] * g + 255 * h
    return r, gg, b

def main():
    random.seed(7)
    data = bytearray(W * H * 2)
    i = 0
    for y in range(H):
        for x in range(W):
            r, g, b = pixel(x, y)
            # Tramado: +-1 nivel de 8 bits antes de cuantizar a 5/6/5.
            n = random.random() * 2 - 1
            r = min(255, max(0, int(r + n * 1.5 + 0.5)))
            g = min(255, max(0, int(g + n * 1.5 + 0.5)))
            b = min(255, max(0, int(b + n * 1.5 + 0.5)))
            p = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            data[i] = p >> 8; data[i + 1] = p & 0xFF
            i += 2

    if "--preview" in sys.argv:
        from PIL import Image
        im = Image.new("RGB", (W, H))
        px = im.load()
        for y in range(H):
            for x in range(W):
                p = (data[(y * W + x) * 2] << 8) | data[(y * W + x) * 2 + 1]
                px[x, y] = (((p >> 11) & 31) * 255 // 31, ((p >> 5) & 63) * 255 // 63, (p & 31) * 255 // 31)
        im.save(sys.argv[sys.argv.index("--preview") + 1])

    lines = []
    for k in range(0, len(data), 24):
        lines.append("  " + ",".join(f"0x{b:02X}" for b in data[k:k + 24]) + ",")
    OUT.write_text(
        "// Generado por tools/make_wallpaper.py. No editar a mano.\n"
        "// 480x320 RGB565, bytes en big-endian (LV_COLOR_16_SWAP = 1).\n"
        "#include <lvgl.h>\n\n"
        "static const LV_ATTRIBUTE_MEM_ALIGN uint8_t wallpaper_map[] = {\n"
        + "\n".join(lines) +
        "\n};\n\n"
        "const lv_img_dsc_t wallpaper = {\n"
        "  .header = { .cf = LV_IMG_CF_TRUE_COLOR, .always_zero = 0, .reserved = 0,\n"
        f"              .w = {W}, .h = {H} }},\n"
        f"  .data_size = {len(data)},\n"
        "  .data = wallpaper_map,\n"
        "};\n")
    print(f"{OUT}  {len(data)} bytes")

if __name__ == "__main__":
    main()
