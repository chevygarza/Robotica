#!/bin/bash
# VNL-1 — Doble clic para sincronizar la microSD y flashear la perilla.
cd "$(dirname "$0")"
clear
echo "───────────────────────────────────────────────"
echo "  VinilOS — actualizando musica y firmware"
echo "───────────────────────────────────────────────"
echo
PY="sd/.venv/bin/python"
[ -x "$PY" ] || PY="python3"
"$PY" sd/prepare_sd.py --flash
echo
echo "───────────────────────────────────────────────"
echo "  Puedes cerrar esta ventana."
echo "───────────────────────────────────────────────"
