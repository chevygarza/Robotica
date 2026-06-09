# Restaurar StuntHub v2 (binario exportado 2026-06-09)

7 apps: Clima MTY, X @stuntech, Luces Hue, Mercados, Servidor Mac Mini,
Wallpapers (DBZ/Pokemon/Zelda full screen), PC Gamer (prender WoL + apagar
via pc_agent). Incluye sleep 30s, brillo nocturno, arranque escalonado.

Con la placa conectada (ver puerto con `ls /dev/cu.usbmodem*`):

```bash
cd ~/Desktop/crowpanel-esp32s3/firmware/StuntHub
arduino-cli upload -p /dev/cu.usbmodemXXXX \
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" \
  --input-dir ~/Desktop/crowpanel-esp32s3/backup/stunthub_v2_bin
```

(Equivalente: recompilar desde firmware/StuntHub — el código está en git.)
NOTA: stunthub_v1_bin/ es la versión vieja sin PC Gamer; esta v2 es la buena.
