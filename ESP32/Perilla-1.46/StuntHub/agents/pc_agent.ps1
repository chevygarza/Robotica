# ============================================================
#  PC Agent - control de la PC gamer desde StuntHub (LAN)
#  GET  /status?t=TOKEN   -> {"online":true,"uptime_min":N}
#  POST /shutdown?t=TOKEN -> apaga en 5 seg
#  POST /cancel?t=TOKEN   -> cancela un apagado pendiente
#  POST /normal|/sim|/tv  -> dispara el perfil de monitores+audio
#
#  GOTCHA: este agente corre como SYSTEM (sesion 0). Los perfiles cambian
#  DISPLAY/AUDIO y abren Steam = requieren la sesion interactiva del usuario.
#  Por eso /normal|/sim|/tv NO corren el .ps1 directo: disparan tareas
#  programadas (StuntHub-Normal/Sim/TV) creadas con /it /ru <usuario>, que
#  SI corren en la sesion logueada. Ver INSTALL.md.
# ============================================================
param(
  [int]$Port = 8767,
  [string]$Token = "stunthub"   # debe coincidir con GAMER_TOKEN en StuntHub
)

$ErrorActionPreference = 'SilentlyContinue'

$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://+:$Port/")
$listener.Start()
Write-Host "PC Agent escuchando en puerto $Port"

while ($listener.IsListening) {
  $ctx = $listener.GetContext()
  $req = $ctx.Request; $res = $ctx.Response
  $body = '{"error":"not_found"}'; $res.StatusCode = 404

  try {
    if ($req.QueryString["t"] -ne $Token) {
      $body = '{"error":"auth"}'; $res.StatusCode = 403
    }
    elseif ($req.HttpMethod -eq 'GET' -and $req.Url.AbsolutePath -eq '/status') {
      $up = (Get-Date) - (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
      $body = '{"online":true,"uptime_min":' + [int]$up.TotalMinutes + '}'
      $res.StatusCode = 200
    }
    elseif ($req.HttpMethod -eq 'POST' -and $req.Url.AbsolutePath -eq '/shutdown') {
      shutdown /s /t 5 /c "Apagada desde StuntHub. Buenas noches, gamer."
      $body = '{"ok":true}'; $res.StatusCode = 200
    }
    elseif ($req.HttpMethod -eq 'POST' -and $req.Url.AbsolutePath -eq '/cancel') {
      shutdown /a
      $body = '{"ok":true}'; $res.StatusCode = 200
    }
    elseif ($req.HttpMethod -eq 'POST' -and $req.Url.AbsolutePath -eq '/normal') {
      Start-ScheduledTask -TaskName "StuntHub-Normal"
      $body = '{"ok":true}'; $res.StatusCode = 200
    }
    elseif ($req.HttpMethod -eq 'POST' -and $req.Url.AbsolutePath -eq '/sim') {
      Start-ScheduledTask -TaskName "StuntHub-Sim"
      $body = '{"ok":true}'; $res.StatusCode = 200
    }
    elseif ($req.HttpMethod -eq 'POST' -and $req.Url.AbsolutePath -eq '/tv') {
      Start-ScheduledTask -TaskName "StuntHub-TV"
      $body = '{"ok":true}'; $res.StatusCode = 200
    }
  } catch { $body = '{"error":"internal"}'; $res.StatusCode = 500 }

  $res.ContentType = 'application/json'
  $buf = [System.Text.Encoding]::UTF8.GetBytes($body)
  $res.ContentLength64 = $buf.Length
  $res.OutputStream.Write($buf, 0, $buf.Length)
  $res.OutputStream.Close()
}
