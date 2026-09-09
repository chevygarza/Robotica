# Setup de una Mac nueva — proyectos de Jose

Objetivo: que cualquier Mac (y el server) tenga los proyectos en el mismo lugar,
con el mismo nombre, y que Claude Code siga las mismas reglas en todas.

## La regla (una linea)
**Todo el codigo vive en `~/Developer/<nombre-exacto-del-repo-en-GitHub>/`. Nada mas.**
Desktop es para capturas y temporales. Nada suelto en `~`.
Unica excepcion: `~/Developer/Games-Apps/` es el cajon de prototipos sin git
(solo existe en la Mac donde se crearon; si uno madura, se vuelve repo).

## Estructura objetivo
```
~/Developer/
├── Robotica/                            carrito Arduino + perilla + pantallas ESP32
├── WifgetWorld/                         app Warpy (iOS/watchOS)
├── warpy-catalog/
├── App-To-Do-List-familiar-para-iPad/
├── pokemon-portfolio-ios/
├── yt-downloader/                       yt2mp3
├── tme-web/
├── plantel/
├── Orbit-FamilyOS/
├── reSpeaker_XVF3800_USB_4MIC_ARRAY/    clon de terceros (respeaker)
└── Games-Apps/                          prototipos SIN git (PixelRetro, aventura-en-monterrey)
```
Clona solo los que vayas a usar en esa Mac; la ruta y el nombre no cambian.

## Pasos en una Mac nueva

### 1. Herramientas base
```bash
xcode-select --install
brew install gh
gh auth login          # cuenta chevygarza, HTTPS
git config --global user.name "Jose Garza"
git config --global user.email "c.egarza@gmail.com"
```

### 2. Carpeta y repos
```bash
mkdir -p ~/Developer && cd ~/Developer
gh repo clone chevygarza/Robotica
gh repo clone chevygarza/WifgetWorld
gh repo clone chevygarza/yt-downloader
# ...los demas que hagan falta:  gh repo list chevygarza
```

### 3. Reglas globales para Claude Code
Crea `~/.claude/CLAUDE.md` con este contenido (Claude Code lo lee en todos los
proyectos de esa Mac):
```bash
mkdir -p ~/.claude && cat > ~/.claude/CLAUDE.md <<'EOF2'
# Reglas globales de Jose (aplican a todos los proyectos en esta Mac)

## Donde vive el codigo
- TODO el codigo vive en `~/Developer/<nombre-exacto-del-repo-en-GitHub>/`.
  Nunca en Desktop, Documents ni suelto en `~`. Un proyecto = un repo = una carpeta.
- Un proyecto nuevo se crea en `~/Developer/`, con `git init` y repo en GitHub
  (cuenta `chevygarza`, privado por defecto) el mismo dia. Nada vive solo local.
- Desktop es zona temporal: capturas, descargas, pruebas. Nunca codigo.
- Excepcion: `~/Developer/Games-Apps/` es el cajon de prototipos y primeros
  proyectos, SIN git. Cuando uno madure, sale a `~/Developer/<repo>` con GitHub.
- Se trabaja desde varias Macs: la ruta `~/Developer/<repo>` es identica en todas.

## Sincronizacion (contrato con cualquier sesion)
1. Al empezar una sesion en un repo: `git pull` antes de tocar nada.
2. Al terminar cada cambio: `git add -A && git commit && git push`.
3. Conflictos de `git pull` se resuelven ANTES de seguir.
4. Nunca subir secretos (`secrets.h`, `.env`, tokens) ni toolchains/binarios.

## Convenciones
- Cada repo tiene su `CLAUDE.md` en la raiz; leerlo antes de tocar el proyecto.
- Rutas en documentacion: relativas a la raiz del repo, nunca `~/Desktop/...`.
- Sketches de Arduino: la carpeta se llama igual que el `.ino`; no renombrar.
EOF2
```

### 4. Lo que NO viaja con git (por proyecto)
| Proyecto | Que falta en una Mac nueva | Donde dice como |
|---|---|---|
| Robotica / Perilla-1.46 | core esp32 2.0.17 + librerias pinneadas, `secrets.h` en StuntHub, TMEhub y VinilOS | `ESP32/Perilla-1.46/build_env/README.md` |
| Robotica / Pantalla-3.5 | core esp32 3.3.11 aislado en `tools/`, `secrets.h` en `firmware/TAC1` | `ESP32/Pantalla-3.5/CLAUDE.md` |
| Robotica / Carrito | Arduino IDE o arduino-cli con core AVR | `Carrito-Arduino/.../README.txt` |
| WifgetWorld | Xcode + XcodeGen (`project.yml` genera el `.xcodeproj`) | `README.md` del repo |
| yt-downloader | `python3 -m venv .venv && .venv/bin/pip install -r requirements.txt` | `README.md` |

Los `secrets.h` se copian desde `secrets.h.example` y se llenan a mano. Nunca se suben.

## Rutina diaria (en cualquier Mac)
```bash
cd ~/Developer/<repo>
git pull                 # antes de empezar
# ... trabajar ...
git add -A && git commit -m "..." && git push    # al terminar
```
Si al hacer `git pull` hay conflictos, se resuelven antes de escribir codigo nuevo.

## Migrar una Mac que ya tiene proyectos regados
1. `find ~ -maxdepth 3 -name .git -type d` para ubicar repos sueltos.
2. `mv` cada uno a `~/Developer/<nombre-del-repo>` (ver `git remote -v` para el nombre).
3. Proyectos sin git: `git init`, `.gitignore`, `gh repo create <nombre> --private --source=. --push`.
4. Cerrar y reabrir Xcode / Claude Code desde la ruta nueva.
