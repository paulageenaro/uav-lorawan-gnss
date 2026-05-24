# 🛸 Exportador de Datos de InfluxDB a CSV y JSON (TFG)

Este conjunto de utilidades permite descargar los datos históricos de tu base de datos **InfluxDB v2** para el bucket `lorawan-gps-uav` y la mota Heltec (`a4cf123456789a01`), y convertirlos directamente a formato **JSON** de forma automática y robusta.

## 📁 Archivos creados

1. **`query.flux`**: Archivo de consulta Flux con los filtros para tu mota Heltec y mediciones del dron.
2. **`export_influx.py`**: Script de Python que automatiza la consulta HTTP a InfluxDB, descarga el CSV y realiza una conversión premium a JSON (manteniendo tipos de datos numéricos y booleanos nativos sin dependencias).
3. **`export_influx.ps1`**: Script de PowerShell para ejecutar todo el flujo desde la terminal de Windows en un solo comando.
4. **`.env.example`**: Plantilla de configuración para tus credenciales.

---

## 🛠️ Cómo configurarlo y usarlo

### Paso 1: Configurar tus credenciales
1. Haz una copia del archivo `.env.example` y cámbiale el nombre a `.env`.
2. Abre `.env` con un editor de texto y pega tu token real de InfluxDB en la variable `INFLUX_TOKEN`:
   ```env
   INFLUX_URL=http://localhost:8086
   INFLUX_ORG=rim-org
   INFLUX_TOKEN=TU_TOKEN_AQUI
   ```

### Paso 2: Ejecutar la exportación

#### Opción A: Desde PowerShell (Recomendado)
Abre PowerShell en esta carpeta y ejecuta:
```powershell
.\export_influx.ps1
```
*Si tienes problemas de políticas de ejecución en tu PowerShell, puedes ejecutarlo así:*
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\export_influx.ps1
```

#### Opción B: Directamente con Python
También puedes ejecutar directamente el script de Python, el cual te guiará y leerá la configuración de tu archivo `.env`:
```bash
python export_influx.py
```
O especificando parámetros por línea de comandos:
```bash
python export_influx.py --token "TU_TOKEN" --org "rim-org" --url "http://localhost:8086"
```

---

## 📊 Resultado
Una vez ejecutado con éxito cualquiera de los métodos anteriores, se generarán dos archivos en la carpeta:
* 📄 **`chirpstack_export.csv`**: Los datos crudos en formato CSV descargados de InfluxDB.
* 📦 **`chirpstack_export.json`**: El archivo JSON estructurado con los tipos numéricos correctos (listo para ser procesado o importado).
