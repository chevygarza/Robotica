# PC Agent — instalación en la PC GAMER (Windows)

Copiar `pc_agent.ps1` a `C:\StuntHub\` (crear la carpeta).

## 1. Permisos (UNA vez, PowerShell como Administrador)
```powershell
netsh http add urlacl url=http://+:8767/ user=Everyone
New-NetFirewallRule -DisplayName "PC Agent StuntHub" -Direction Inbound -LocalPort 8767 -Protocol TCP -Action Allow -Profile Private
```

## 2. Tarea permanente (arranca con el SISTEMA, antes del login)
```powershell
schtasks /Create /TN "PC Agent StuntHub" /SC ONSTART /RU SYSTEM /RL HIGHEST /TR "powershell -WindowStyle Hidden -ExecutionPolicy Bypass -File C:\StuntHub\pc_agent.ps1" /F
schtasks /Run /TN "PC Agent StuntHub"
```

## 3. Modos Normal/Sim/TV — 3 tareas programadas (UNA vez, PowerShell Admin)

Los perfiles (`C:\TVGaming\profile-*.ps1`) cambian DISPLAY+AUDIO y abren Steam:
requieren la sesión interactiva del usuario. El agente corre como SYSTEM (sesión 0)
y NO puede hacerlo directo. Solución: el agente dispara estas tareas, creadas con
`/it` (sesión interactiva) y `/ru <usuario>` (corren como el usuario logueado).

⚠️ Reemplaza `Chevy` por el usuario real de Windows si es distinto (`whoami`).

```powershell
$U = "Chevy"   # usuario que inicia sesion en la PC gamer
schtasks /Create /TN "StuntHub-Normal" /SC ONCE /ST 00:00 /RL LIMITED /IT /F /RU $U /TR "powershell -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File C:\TVGaming\profile-normal.ps1"
schtasks /Create /TN "StuntHub-Sim"    /SC ONCE /ST 00:00 /RL LIMITED /IT /F /RU $U /TR "powershell -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File C:\TVGaming\profile-sim.ps1"
schtasks /Create /TN "StuntHub-TV"     /SC ONCE /ST 00:00 /RL LIMITED /IT /F /RU $U /TR "powershell -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File C:\TVGaming\profile-tv.ps1"
```
`/SC ONCE /ST 00:00` = nunca se dispara solo; solo cuando el agente hace
`Start-ScheduledTask`. `/IT` = corre en la sesión interactiva (clave para display/audio).

## 4. Reiniciar el agente para que tome los endpoints nuevos
```powershell
schtasks /End /TN "PC Agent StuntHub"; schtasks /Run /TN "PC Agent StuntHub"
```

## 5. Probar
- Estado:  `http://localhost:8767/status?t=stunthub` → JSON.
- Desde la red: `http://192.168.86.58:8767/status?t=stunthub` (IP REAL actual; reservar DHCP).
- Modos (con sesión iniciada): `Invoke-WebRequest -Uri http://localhost:8767/normal -Method POST` → debe cambiar monitores. Igual /sim y /tv.
- ⚠️ `POST /shutdown?t=stunthub` APAGA LA PC EN 5 SEG (probar cuando no estés jugando 😄)

## Notas
- El token (`stunthub`) evita que cualquiera en la red la controle. Si lo cambias aquí,
  cambiarlo también en el secrets.h de StuntHub (GAMER_TOKEN).
- ONSTART + SYSTEM = el agente vive aunque nadie haya iniciado sesión.
- Prender la PC NO usa este agente (es Wake-on-LAN directo del ESP32); apagar y modos sí.
- IP: reservar DHCP en Google Home a 192.168.86.58 (o la que fijes) y que coincida con
  GAMER_IP en secrets.h.
