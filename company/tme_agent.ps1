# ============================================================
#  TME Agent - expone el estado del reseteo por HTTP (LAN)
#  para la perilla (CrowPanel). Espejo de barra_progreso.ps1.
#
#  GET  /status -> JSON {state, pct, eta_s, running, signal...}
#  POST /reset  -> lanza PC26_V1.exe (con anti-duplicado)
#
#  Correr con:  powershell -ExecutionPolicy Bypass -File tme_agent.ps1
#  (ver INSTALL.md para dejarlo permanente con Task Scheduler)
# ============================================================
param(
  [int]$Port = 8766,
  [string]$ExePath    = "C:\Users\admin\Documents\TME MACROS\PC26_V1.exe",
  [string]$SignalFile = "$env:TEMP\TME_BAR_SIGNAL.txt",
  [int]$TotalSec = 110     # mismo TotalSec que barra_progreso.ps1
)

$ErrorActionPreference = 'SilentlyContinue'

function Get-StatusJson {
  $running = [bool](Get-Process -Name "PC26_V1" -ErrorAction SilentlyContinue)

  $sig = 'none'; $sigAge = -1
  if (Test-Path $SignalFile) {
    $raw = Get-Content -Path $SignalFile -ErrorAction SilentlyContinue | Select-Object -Last 1
    if ($raw) { $sig = $raw.Trim().ToLower() }
    $sigAge = [int](((Get-Date) - (Get-Item $SignalFile).LastWriteTime).TotalSeconds)
  }

  # Espejo de la lógica de barra_progreso.ps1
  $state = 'idle'; $pct = 0; $eta = -1; $label = 'En espera'
  if ($sig -eq 'error') {
    $state = 'error'; $pct = -1; $label = 'Error de conexion / datos'
  }
  elseif ($sig -eq 'done') {
    $state = 'done'; $pct = 100; $eta = 0; $label = 'Completado'
  }
  elseif ($sig -eq 'check2') {
    if ($sigAge -ge $TotalSec) { $state = 'done'; $pct = 100; $eta = 0; $label = 'Completado' }
    else {
      $state = 'check2'
      $pct = [int](40 + ($sigAge / $TotalSec) * 60)
      $eta = $TotalSec - $sigAge
      $label = 'Datos OK; descargando...'
    }
  }
  elseif ($sig -eq 'check1') {
    $state = 'check1'; $pct = 40; $label = 'Conexion establecida'
  }
  elseif ($running) {
    $state = 'starting'; $pct = 5; $label = 'Preparando...'
  }

  # Resultado viejo (>10 min) sin proceso corriendo -> en espera
  if (-not $running -and $state -in @('done','error') -and $sigAge -gt ($TotalSec + 600)) {
    $state = 'idle'; $pct = 0; $eta = -1; $label = 'En espera'
  }

  @{
    state = $state; pct = $pct; eta_s = $eta; label = $label
    running = $running; signal = $sig; signal_age_s = $sigAge
    total_sec = $TotalSec; ts = (Get-Date -Format 'yyyy-MM-ddTHH:mm:ss')
  } | ConvertTo-Json -Compress
}

function Start-Reset {
  $running = [bool](Get-Process -Name "PC26_V1" -ErrorAction SilentlyContinue)
  if ($running) { return '{"started":false,"reason":"already_running"}' }
  if (-not (Test-Path $ExePath)) { return '{"started":false,"reason":"exe_not_found"}' }
  Start-Process -FilePath $ExePath
  return '{"started":true}'
}

$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://+:$Port/")
$listener.Start()
Write-Host "TME Agent escuchando en puerto $Port  (GET /status, POST /reset)"

while ($listener.IsListening) {
  $ctx = $listener.GetContext()
  $req = $ctx.Request; $res = $ctx.Response
  $body = '{"error":"not_found"}'; $res.StatusCode = 404

  try {
    if ($req.HttpMethod -eq 'GET' -and $req.Url.AbsolutePath -eq '/status') {
      $body = Get-StatusJson; $res.StatusCode = 200
    }
    elseif ($req.HttpMethod -eq 'POST' -and $req.Url.AbsolutePath -eq '/reset') {
      $body = Start-Reset; $res.StatusCode = 200
    }
  } catch { $body = '{"error":"internal"}'; $res.StatusCode = 500 }

  $res.ContentType = 'application/json'
  $res.Headers.Add('Access-Control-Allow-Origin','*')
  $buf = [System.Text.Encoding]::UTF8.GetBytes($body)
  $res.ContentLength64 = $buf.Length
  $res.OutputStream.Write($buf, 0, $buf.Length)
  $res.OutputStream.Close()
}
