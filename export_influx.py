#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
InfluxDB Data Exporter & JSON Converter
--------------------------------------
Este script descarga datos de InfluxDB v2 utilizando una consulta Flux
y los convierte de forma automática a formato CSV y JSON estructurado.

Desarrollado para el proyecto TFG UAV LoRaWAN GNSS.
"""

import os
import sys
import csv
import json
import argparse
import urllib.request
import urllib.error

# Colores ANSI para una interfaz interactiva premium
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_styled(text, color=Colors.ENDC, bold=False):
    """Imprime texto con estilos en terminales compatibles."""
    # Detecta si la terminal soporta color
    has_color = sys.platform != 'win32' or 'ANSICON' in os.environ or os.environ.get('TERM') == 'xterm-256color'
    
    if sys.stdout.isatty() or has_color:
        style = color
        if bold:
            style += Colors.BOLD
        formatted_text = f"{style}{text}{Colors.ENDC}"
    else:
        formatted_text = text
        
    try:
        # Intentamos imprimir en UTF-8 o codificación del sistema
        print(formatted_text)
    except UnicodeEncodeError:
        # Fallback si el terminal de Windows (como CP1252) no soporta ciertos caracteres unicode (como emojis)
        # Reemplazamos los emojis/caracteres especiales con alternativas legibles en ASCII
        cleaned_text = text
        replacements = {
            "🛸": "[UAV]", "📡": "[Net]", "🏢": "[Org]", "📄": "[Doc]", 
            "⏳": "[...] ", "✅": "[OK] ", "🔄": "[Sync]", "🎉": "[SUCCESS]",
            "⚠️": "[WARN]", "❌": "[ERROR]", "👉": "->", "ℹ️": "[INFO]"
        }
        for emoji, rep in replacements.items():
            cleaned_text = cleaned_text.replace(emoji, rep)
        
        # Limpieza final para cualquier otro carácter unicode
        cleaned_text = cleaned_text.encode('ascii', errors='replace').decode('ascii')
        
        if sys.stdout.isatty() or has_color:
            style = color
            if bold:
                style += Colors.BOLD
            cleaned_text = f"{style}{cleaned_text}{Colors.ENDC}"
        print(cleaned_text)


def load_env(env_path='.env'):
    """Carga variables desde un archivo .env sin dependencias externas."""
    env_vars = {}
    if os.path.exists(env_path):
        with open(env_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if '=' in line:
                    key, val = line.split('=', 1)
                    # Quita comillas simples o dobles alrededor del valor
                    val = val.strip().strip("'\"")
                    env_vars[key.strip()] = val
    return env_vars

def parse_value(val):
    """Convierte cadenas de texto de CSV a tipos nativos de Python (int, float, bool, None)."""
    if val == '':
        return None
    if val.lower() == 'true':
        return True
    if val.lower() == 'false':
        return False
    try:
        if '.' not in val:
            return int(val)
    except ValueError:
        pass
    try:
        return float(val)
    except ValueError:
        pass
    return val

def convert_csv_to_json(csv_path, json_path):
    """Convierte el CSV exportado de InfluxDB a un JSON plano y a otro JSON pivotado por tiempo."""
    records = []
    current_headers = None
    
    if not os.path.exists(csv_path):
        raise FileNotFoundError(f"No se encontró el archivo CSV en: {csv_path}")

    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            # Omitir líneas de anotación de InfluxDB que comienzan con '#'
            if row[0].startswith('#'):
                continue
            
            # También omitimos filas de metadatos si aparecen internamente
            if any(cell.startswith('#') for cell in row if cell):
                continue
                
            # Identificar fila de cabecera estándar de InfluxDB
            if len(row) > 2 and row[1] == 'result' and row[2] == 'table':
                current_headers = [h.strip() for h in row]
                continue
            
            if not current_headers:
                continue
                
            # Mapear fila de datos
            record = {}
            for i, val in enumerate(row):
                if i < len(current_headers):
                    key = current_headers[i]
                    if key: # Omitir claves vacías
                        record[key] = parse_value(val)
            
            # Limpiar columnas internas redundantes de InfluxDB
            for key in ['', 'result', 'table']:
                if key in record:
                    del record[key]
                    
            records.append(record)
            
    # Guardar JSON plano original
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(records, f, indent=2, ensure_ascii=False)
        
    # Crear y guardar la versión estructurada/pivotada por marca de tiempo (_time)
    pivoted_count = 0
    try:
        pivoted_path = json_path.replace('.json', '_pivoted.json')
        by_time = {}
        for r in records:
            t = r.get('_time')
            if not t:
                continue
            if t not in by_time:
                by_time[t] = {
                    '_time': t,
                    'device_name': r.get('device_name'),
                    'dev_eui': r.get('dev_eui'),
                    'application_name': r.get('application_name')
                }
            
            meas = r.get('_measurement')
            val = r.get('_value')
            field = r.get('_field')
            
            # Agrupar las mediciones de forma legible
            if meas == 'device_frmpayload_data_location':
                if field:
                    by_time[t][field] = val
            elif meas == 'device_uplink':
                if field:
                    by_time[t][field] = val
            elif meas:
                # Limpiar el prefijo de Chirpstack
                clean_meas = meas.replace('device_frmpayload_data_', '')
                by_time[t][clean_meas] = val
                
        # Ordenar los registros cronológicamente
        pivoted_list = sorted(list(by_time.values()), key=lambda x: x['_time'])
        pivoted_count = len(pivoted_list)
        
        with open(pivoted_path, 'w', encoding='utf-8') as f:
            json.dump(pivoted_list, f, indent=2, ensure_ascii=False)
            
        # Filtrar registros que cumplan con drone_sdk_raw == 1 y gps_fix_raw == 1
        filtered_list = [
            r for r in pivoted_list 
            if r.get('drone_sdk_raw') == 1 and r.get('gps_fix_raw') == 1
        ]
        filtered_count = len(filtered_list)
        filtered_path = json_path.replace('.json', '_filtered.json')
        
        with open(filtered_path, 'w', encoding='utf-8') as f:
            json.dump(filtered_list, f, indent=2, ensure_ascii=False)
            
    except Exception as e:
        print_styled(f"⚠️ Nota: No se pudo generar el archivo pivotado/filtrado: {str(e)}", Colors.WARNING)
        filtered_count = 0
        
    return len(records), pivoted_count, filtered_count



def main():
    print_styled("🛸 INFLUXDB DATA EXPORTER - TFG UAV LoRaWAN", Colors.HEADER, bold=True)
    print_styled("==================================================", Colors.OKBLUE)
    
    # Cargar variables del entorno o .env local
    env_vars = load_env()
    
    # Parser de argumentos
    parser = argparse.ArgumentParser(description="Exporta datos de InfluxDB a CSV y JSON.")
    parser.add_argument('--token', default=env_vars.get('INFLUX_TOKEN'), help="Token de API de InfluxDB")
    parser.add_argument('--org', default=env_vars.get('INFLUX_ORG', 'rim-org'), help="Organización de InfluxDB")
    parser.add_argument('--url', default=env_vars.get('INFLUX_URL', 'http://localhost:8086'), help="URL de InfluxDB")
    parser.add_argument('--query-file', default='query.flux', help="Archivo que contiene la consulta Flux")
    parser.add_argument('--out-csv', default='chirpstack_export.csv', help="Ruta del archivo CSV de salida")
    parser.add_argument('--out-json', default='chirpstack_export.json', help="Ruta del archivo JSON de salida")
    
    args = parser.parse_args()
    
    # Validaciones y modo interactivo si falta información crítica
    token = args.token
    if not token:
        print_styled("⚠️ No se ha detectado el token de InfluxDB en las variables o argumentos.", Colors.WARNING)
        token = input("👉 Introduce tu API Token de InfluxDB: ").strip()
        if not token:
            print_styled("❌ Error: Se requiere un token de InfluxDB para realizar la consulta.", Colors.FAIL, bold=True)
            sys.exit(1)
            
    org = args.org
    url = args.url
    
    # Leer consulta Flux
    if not os.path.exists(args.query_file):
        # Crear consulta por defecto si no existe
        print_styled(f"ℹ️ Archivo {args.query_file} no encontrado. Creando consulta por defecto...", Colors.OKCYAN)
        default_flux = (
            'from(bucket: "lorawan-gps-uav")\n'
            '  |> range(start: -30d)\n'
            '  |> filter(fn: (r) => r["dev_eui"] == "a4cf123456789a01")\n'
            '  |> filter(fn: (r) =>\n'
            '    r["_measurement"] == "device_uplink" or\n'
            '    r["_measurement"] =~ /^device_frmpayload_data/\n'
            '  )'
        )
        with open(args.query_file, 'w', encoding='utf-8') as f:
            f.write(default_flux)
        flux_query = default_flux
    else:
        with open(args.query_file, 'r', encoding='utf-8') as f:
            flux_query = f.read()
            
    print_styled(f"\n📡 Conectando a InfluxDB en: {url}", Colors.OKCYAN)
    print_styled(f"🏢 Org: {org} | 📄 Query File: {args.query_file}", Colors.OKCYAN)
    
    # Endpoint de consulta
    query_url = f"{url.rstrip('/')}/api/v2/query?org={org}"
    
    req = urllib.request.Request(
        query_url,
        data=flux_query.encode('utf-8'),
        headers={
            'Authorization': f'Token {token}',
            'Accept': 'application/csv',
            'Content-type': 'application/vnd.flux'
        },
        method='POST'
    )
    
    print_styled("\n⏳ Descargando datos desde la API de InfluxDB...", Colors.OKBLUE)
    
    try:
        with urllib.request.urlopen(req) as response:
            csv_data = response.read()
            
        # Escribir el CSV crudo
        with open(args.out_csv, 'wb') as f:
            f.write(csv_data)
            
        print_styled(f"✅ CSV descargado con éxito y guardado en: {args.out_csv}", Colors.OKGREEN, bold=True)
        
        # Convertir a JSON
        print_styled("🔄 Convirtiendo datos a JSON estructurado, pivotado y filtrado...", Colors.OKBLUE)
        num_records, num_pivoted, num_filtered = convert_csv_to_json(args.out_csv, args.out_json)
        
        piv_file = args.out_json.replace('.json', '_pivoted.json')
        filt_file = args.out_json.replace('.json', '_filtered.json')
        print_styled(f"🎉 ¡Éxito! Se han procesado {num_records} métricas individuales.", Colors.OKGREEN, bold=True)
        print_styled(f"📂 Archivo JSON plano guardado en: {args.out_json}", Colors.OKGREEN)
        print_styled(f"📂 Archivo JSON pivotado (GPS + Dron consolidados) guardado en: {piv_file} ({num_pivoted} tramas)", Colors.OKGREEN)
        print_styled(f"📂 Archivo JSON filtrado (Solo Dron en vuelo + GPS Fix) guardado en: {filt_file} ({num_filtered} tramas útiles)", Colors.OKGREEN, bold=True)
        
    except urllib.error.HTTPError as e:
        print_styled(f"\n❌ Error de API de InfluxDB (HTTP {e.code})", Colors.FAIL, bold=True)
        try:
            error_body = e.read().decode('utf-8')
            error_json = json.loads(error_body)
            print_styled(f"💬 Detalle: {error_json.get('message', error_body)}", Colors.FAIL)
        except Exception:
            print_styled(f"💬 Código de estado: {e.reason}", Colors.FAIL)
        sys.exit(1)
        
    except urllib.error.URLError as e:
        print_styled(f"\n❌ Error de red al intentar conectar a InfluxDB", Colors.FAIL, bold=True)
        print_styled(f"💬 Detalle: {e.reason}", Colors.FAIL)
        print_styled("👉 Asegúrate de que el servidor local de InfluxDB esté encendido (http://localhost:8086)", Colors.WARNING)
        sys.exit(1)
        
    except Exception as e:
        print_styled(f"\n❌ Error inesperado: {str(e)}", Colors.FAIL, bold=True)
        sys.exit(1)

if __name__ == '__main__':
    main()
