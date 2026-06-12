# TME Agent — instalación en la máquina de reseteos (Windows)

Copiar `tme_agent.ps1` a `C:\Users\admin\Documents\TME MACROS\`.

## 1. Permisos (UNA vez, PowerShell como Administrador)
```powershell
# Permitir que el agente escuche en el puerto 8766
netsh http add urlacl url=http://+:8766/ user=Everyone

# Abrir el puerto en el firewall (solo red local)
New-NetFirewallRule -DisplayName "TME Agent" -Direction Inbound -LocalPort 8766 -Protocol TCP -Action Allow -Profile Private
```

## 2. Probar a mano
```powershell
powershell -ExecutionPolicy Bypass -File "C:\Users\admin\Documents\TME MACROS\tme_agent.ps1"
```
Desde otra máquina de la red:
- `http://<IP-de-la-maquina>:8766/status`  → debe responder JSON.
- Correr un reseteo normal (icono) y refrescar /status → debe verse check1/check2/pct avanzando.

## 3. Dejarlo permanente (arranca con la máquina)
```powershell
schtasks /Create /TN "TME Agent" /SC ONLOGON /RL HIGHEST ^
  /TR "powershell -WindowStyle Hidden -ExecutionPolicy Bypass -File \"C:\Users\admin\Documents\TME MACROS\tme_agent.ps1\""
```

## 4. (Opcional, 1 línea) señal 'done' explícita
En el macro Jitbit, en el camino de ÉXITO (antes de `listo_reseteo.ps1`), agregar:
```
OPEN FILE  powershell.exe  -WindowStyle Hidden -NoProfile -Command "Set-Content -Path $env:TEMP\TME_BAR_SIGNAL.txt -Value 'done'"
```
Sin esto también funciona (el agente infiere 'done' cuando check2 cumple sus 110s),
pero con la señal explícita el "Completado" es exacto.

## Endpoints
| Método | Ruta | Respuesta |
|---|---|---|
| GET  | /status | `{state: idle\|starting\|check1\|check2\|done\|error, pct, eta_s, label, running, ...}` |
| POST | /reset  | `{started:true}` o `{started:false, reason:"already_running"}` |

## Pendiente
- Reserva DHCP para esta máquina en el módem (que su IP no cambie).
- Anotar la IP para configurarla en la perilla (ResetHub).
