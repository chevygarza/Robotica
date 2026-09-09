# Robotica — workspace de Jose

Repo unico con todos los proyectos de hardware. Se trabaja desde varias Macs
con Claude Code, asi que la sincronizacion con GitHub es obligatoria.

## ⚠️ Regla de sincronizacion (contrato con cualquier sesion)
1. **Al empezar** cualquier sesion: `git pull` antes de tocar nada.
2. **Al terminar** cada cambio: `git add -A && git commit && git push`.
   No se deja trabajo sin subir al cerrar la sesion.
3. Si `git pull` trae conflictos, se resuelven ANTES de seguir trabajando.
4. Nunca subir `secrets.h` ni binarios de toolchain (ya estan en `.gitignore`).

## Mapa de proyectos (que es cada cosa)

| Carpeta | Dispositivo | Que es |
|---|---|---|
| `Carrito-Arduino/SmartRobotCarV4.0_Stuntech/` | Carrito Elegoo Smart Robot Car V4.0 (Arduino UNO) | Firmware del carrito: motores TB6612, giroscopio GY-521, ultrasonido, IR |
| `ESP32/Perilla-1.46/` | Perilla Elecrow CrowPanel 1.46" redonda (ESP32-S3) | Workspace con TRES sistemas para el mismo hardware, ver abajo |
| `ESP32/Pantalla-3.5/` | Pantalla tactil 3.5" Guition JC3248W535 (ESP32-S3) | TAC-1 / TactOS: hub de apps tactil. Cimientos listos, nada fabricado aun |
| `ESP32/Pantalla-1.8/` | Pantalla 1.8" (ESP32) | Recien conectada. Sin codigo. Ver su CLAUDE.md para arrancar |

### Los tres sistemas de la perilla (`ESP32/Perilla-1.46/`)
| Carpeta | Sistema | Para que |
|---|---|---|
| `StuntHub/` | **StuntHub** | Perilla PERSONAL de Jose: musica (VinilOS integrado) + apps de apoyo |
| `TMEhub/` | **TME** | Perilla INDUSTRIAL para la empresa TME: reseteos de camiones |
| `VinilOS/` | **VinilOS** (VNL-1) | Reproductor tipo tornamesa. Proyecto original; su motor ya vive dentro de StuntHub |

Los nombres de las carpetas de sketch (`StuntHub/`, `TMEhub/`, `VinilOS/firmware/VNL1/`)
NO se renombran: Arduino exige que el `.ino` se llame igual que su carpeta.

## Convenciones
- Cada proyecto tiene su propio `CLAUDE.md` con reglas duras, hardware y build.
  **Leerlo antes de tocar ese proyecto.** El de `ESP32/Perilla-1.46/CLAUDE.md`
  tiene las reglas duras que heredan los demas aparatos ESP32.
- Rutas en la documentacion: relativas a la raiz de este repo.
- Toolchains y librerias descargadas viven fuera de git (`tools/`, `build/`).
  En una Mac nueva se instalan siguiendo el CLAUDE.md o README de cada proyecto.
- `secrets.h` (Wi-Fi, tokens) se copia a mano en cada Mac desde `secrets.h.example`.
- Nuevo aparato = nueva carpeta bajo `ESP32/` con nombre `Tipo-Tamaño`
  (ej. `Pantalla-1.8`) y su `CLAUDE.md` desde el primer dia.
