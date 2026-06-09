# Restaurar StuntHub (binario exportado 2026-06-09, 6 apps)

Con la placa conectada (ver puerto con `ls /dev/cu.usbmodem*`):

```bash
cd ~/Desktop/crowpanel-esp32s3/firmware/StuntHub
arduino-cli upload -p /dev/cu.usbmodemXXXX \
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,USBMode=hwcdc,CDCOnBoot=cdc" \
  --input-dir ~/Desktop/crowpanel-esp32s3/backup/stunthub_v1_bin
```

(O recompilar desde el código en firmware/StuntHub — mismo resultado, está en git.)
