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

## 3. Probar
- En la misma PC: `http://localhost:8767/status?t=stunthub` → debe responder JSON.
- Desde otra máquina: `http://192.168.86.70:8767/status?t=stunthub`
- ⚠️ `POST /shutdown?t=stunthub` APAGA LA PC EN 5 SEG (probar cuando no estés jugando 😄)

## Notas
- El token (`stunthub`) evita que cualquiera en la red la apague. Si lo cambias aquí,
  cambiarlo también en el secrets.h de StuntHub (GAMER_TOKEN).
- ONSTART + SYSTEM = el agente vive aunque nadie haya iniciado sesión.
- Prender la PC NO usa este agente (eso es Wake-on-LAN directo); esto es solo para apagar.
