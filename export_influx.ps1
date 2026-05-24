# Script de PowerShell para exportar datos de InfluxDB y convertirlos a JSON

# 1. Configuración por defecto (Edita estos campos o usa un archivo .env)
$URL = "http://localhost:8086"
$ORG = "rim-org"
$TOKEN = "PEGA_AQUI_TU_TOKEN"

# Intentar cargar variables desde un archivo .env si existe en la misma carpeta
$EnvPath = Join-Path $PSScriptRoot ".env"
if (Test-Path $EnvPath) {
    Write-Host "ℹ️ Cargando variables de configuración desde $EnvPath..." -ForegroundColor Cyan
    Get-Content $EnvPath | ForEach-Object {
        $line = $_.Trim()
        if ($line -and -not $line.StartsWith("#") -and $line.Contains("=")) {
            $key, $value = $line.Split("=", 2)
            $key = $key.Trim()
            $value = $value.Trim().Trim("'").Trim('"')
            if ($key -eq "INFLUX_URL") { $URL = $value }
            elseif ($key -eq "INFLUX_ORG") { $ORG = $value }
            elseif ($key -eq "INFLUX_TOKEN") { $TOKEN = $value }
        }
    }
}

# Verificar si el token sigue siendo el de ejemplo
if ($TOKEN -eq "PEGA_AQUI_TU_TOKEN" -or $TOKEN -eq "") {
    Write-Host "⚠️ No se ha configurado un Token de InfluxDB real." -ForegroundColor Yellow
    $TOKEN = Read-Host "👉 Introduce tu API Token de InfluxDB"
    if (-not $TOKEN) {
        Write-Host "❌ Error: Se requiere un token para realizar la exportación." -ForegroundColor Red
        exit 1
    }
}

Write-Host "📡 Conectando a InfluxDB en: $URL (Organización: $ORG)..." -ForegroundColor Cyan

# Comprobar si existe el archivo query.flux
$QueryFile = Join-Path $PSScriptRoot "query.flux"
if (-not (Test-Path $QueryFile)) {
    Write-Host "❌ Error: No se encuentra el archivo de consulta 'query.flux' en la ruta: $QueryFile" -ForegroundColor Red
    exit 1
}

# Ejecutar el comando curl.exe para descargar el CSV
Write-Host "⏳ Descargando datos en formato CSV..." -ForegroundColor Yellow
curl.exe -X POST "$URL/api/v2/query?org=$ORG" `
  -H "Authorization: Token $TOKEN" `
  -H "Accept: application/csv" `
  -H "Content-type: application/vnd.flux" `
  --data-binary "@$QueryFile" `
  -o chirpstack_export.csv

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Error al descargar los datos desde InfluxDB (curl falló con código $LASTEXITCODE)." -ForegroundColor Red
    exit 1
}

Write-Host "✅ CSV descargado con éxito y guardado como 'chirpstack_export.csv'." -ForegroundColor Green

# Convertir el CSV a JSON utilizando el script Python
Write-Host "🔄 Convirtiendo CSV a JSON estructurado..." -ForegroundColor Yellow

$PythonScript = Join-Path $PSScriptRoot "export_influx.py"
if (Test-Path $PythonScript) {
    # Ejecutar la conversión pasándole los archivos de entrada/salida
    python.exe $PythonScript --token $TOKEN --org $ORG --url $URL
} else {
    # Fallback si por alguna razón no está el script, usar una línea en línea
    Write-Host "ℹ️ Ejecutando script de conversión inline en Python..." -ForegroundColor Cyan
    python.exe -c "
import csv, json, os
records = []
headers = None
if os.path.exists('chirpstack_export.csv'):
    with open('chirpstack_export.csv', 'r', encoding='utf-8') as f:
        for r in csv.reader(f):
            if r and not r[0].startswith('#'):
                if len(r) > 2 and r[1] == 'result' and r[2] == 'table':
                    headers = [h.strip() for h in r]
                    continue
                if headers:
                    rec = {}
                    for i in range(min(len(headers), len(r))):
                        h = headers[i]
                        if h not in ['', 'result', 'table']:
                            val = r[i]
                            # Intentar parsear tipos nativos
                            if val.lower() == 'true': val = True
                            elif val.lower() == 'false': val = False
                            else:
                                try:
                                    if '.' in val: val = float(val)
                                    else: val = int(val)
                                except ValueError: pass
                            rec[h] = val
                    records.append(rec)
    with open('chirpstack_export.json', 'w', encoding='utf-8') as f:
        json.dump(records, f, indent=2, ensure_ascii=False)
    print('✅ JSON generado con éxito en chirpstack_export.json.')
else:
    print('❌ Error: No se encontró chirpstack_export.csv.')
"
}

Write-Host "🎉 Proceso completado con éxito. ¡Ya tienes tus archivos listos!" -ForegroundColor Green
