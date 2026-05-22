# Dashboards Grafana del TFG LoRaWAN-UAV

## Descripción general

Esta carpeta contiene los dashboards de Grafana utilizados en el Trabajo de Fin de Grado (TFG) sobre el diseño y evaluación de una red UAV basada en LoRaWAN para entornos de emergencia. Los dashboards permiten visualizar en tiempo real la información almacenada en InfluxDB, procedente de ChirpStack, que actúa como servidor de red LoRaWAN y reenvía los datos de los dispositivos al bucket `lorawan-gps-uav` de InfluxDB.

El sistema utiliza dos motas LoRaWAN con roles diferenciados dentro del TFG, y los dashboards están organizados para reflejar esta distinción.

---

## Dashboards incluidos

### 1. `dashboard_linkone_gnss.json` — Dashboard LINK ONE

**UID:** `tfg-linkone-gnss-dashboard`

Este dashboard centraliza todos los datos de la mota **LINK ONE 13** (DevEUI: `000000000000100d`).

**Finalidad dentro del TFG:**
Se utiliza como **mota de apoyo para validar la visualización geográfica en Grafana**. A diferencia de la Heltec, las coordenadas GNSS de LINK ONE (latitud, longitud y geohash) se almacenan correctamente en InfluxDB, lo que permite representar su posición en el mapa de Grafana. Este dashboard valida que el flujo completo ChirpStack → InfluxDB → Grafana funciona correctamente para los datos GNSS.

**Paneles incluidos:**
- Panel de descripción de la mota LINK ONE (markdown)
- Payload counter de Cayenne canal 1 (stat)
- Disponibilidad del GPS (stat, indicando si es real o prueba)
- Última latitud, longitud y altitud (stat)
- frame number (fCnt), RSSI y SNR (stat y gauges)
- Evolución temporal de latitud, longitud y altitud (series temporales)
- Evolución temporal de fCnt, RSSI y SNR (series temporales)
- Histogramas de distribución de RSSI y SNR
- Mapas de cobertura interactivos de SNR y RSSI cruzados con GPS (geomaps)
- Tabla de diagnóstico con los últimos valores recibidos


**Fuente de los datos GNSS:**
El dashboard acepta coordenadas tanto si aparecen como medidas tipo `device_frmpayload_data_latitude` / `device_frmpayload_data_longitude` / `device_frmpayload_data_geohash`, como si aparecen como campos dentro de `device_uplink` con `_field` = `latitude`, `longitude` o `geohash`.

**Codec en ChirpStack:**
LINK ONE 13 utiliza el codec **Cayenne LPP** (Low Power Payload), seleccionable directamente en ChirpStack como opción integrada (`CayenneLPP` en la pestaña Codec del Device Profile). Este formato estándar incluye latitud, longitud y altitud codificadas automáticamente, lo que explica por qué sus coordenadas GNSS aparecen correctamente en InfluxDB sin necesidad de un decoder JavaScript personalizado.

---

### 2. `dashboard_heltec_lorawan_dron.json` — Dashboard Heltec

**UID:** `tfg-heltec-lorawan-dron-dashboard`

Este dashboard está dedicado a la **Heltec Wireless Tracker** (DevEUI: `a4cf123456789a01`), la mota principal del sistema UAV-LoRaWAN del TFG.

**Finalidad dentro del TFG:**
Se utiliza para analizar la mota principal, que es la encargada de recibir y transmitir por LoRaWAN tanto los datos GNSS del propio dispositivo como las métricas del dron (batería, distancia ToF, velocidad, tiempo de vuelo y estado del SDK). Permite evaluar la calidad del enlace radio (RSSI, SNR, fCnt) y el comportamiento de los sensores integrados.

**Paneles incluidos:**
- RSSI, SNR y fCnt de Heltec (series temporales)
- Última batería del dron (stat con umbrales de color)
- Último ToF del dron en centímetros (stat)
- Última velocidad del dron (stat)
- Tiempo de vuelo del dron en segundos (stat)
- Evolución temporal de: batería, ToF, velocidad, SDK raw y tiempo del dron
- Diagnóstico GNSS en InfluxDB: latitud, longitud y altitud
- Diagnóstico de satélites GNSS y GPS fix raw
- Tabla completa de los últimos valores recibidos en InfluxDB
- Panel de texto con nota sobre la limitación GNSS

> ⚠️ **Nota importante:** La Heltec es la mota principal del sistema. Los eventos de ChirpStack muestran coordenadas GNSS válidas, pero en las pruebas actuales dichas coordenadas **no siempre aparecen almacenadas en InfluxDB**. Por ello, este dashboard incluye paneles de diagnóstico para comprobar la persistencia de los campos de posición. Si los paneles de latitud/longitud aparecen vacíos, se debe revisar la integración ChirpStack → InfluxDB (payload decoder, integración HTTP/MQTT, configuración del bucket).

---

### 3. `dashboard_conjunto_heltec_linkone_dron.json` — Dashboard conjunto

**UID:** `tfg-conjunto-heltec-linkone-dron`

Este dashboard ofrece una visión global de ambas motas y de las métricas del dron en una única interfaz de monitorización.

**Finalidad dentro del TFG:**
Permite monitorizar el sistema completo durante las sesiones de prueba, comparando el comportamiento de los dos dispositivos LoRaWAN y verificando que las métricas del dron se están recibiendo correctamente a través de la Heltec. Es especialmente útil para detectar diferencias en la calidad del enlace entre dispositivos y para tener una visión de conjunto del sistema UAV-LoRaWAN.

**Paneles incluidos:**
- RSSI, SNR y fCnt de **Heltec** (series temporales, color ámbar)
- RSSI, SNR y fCnt de **LINK ONE** (series temporales, color azul)
- Mapa multi-capa interactivo de trayectorias (Heltec + LINK ONE) cruzado con RSSI y SNR (geomap)
- Diagnóstico GNSS de Heltec (latitud, longitud y altitud en series temporales)
- Diagnóstico y geolocalización de **LINK ONE**: payload counter, disponibilidad GPS, y stat panels para última latitud, longitud y altitud
- Métricas del dron recibidas por la Heltec: batería (%), distancia ToF (cm), velocidad, tiempo (s) y SDK raw en stat y series temporales
- Tablas de diagnóstico con los últimos valores de Heltec y LINK ONE
- Panel de resumen explicativo sobre el funcionamiento del sistema y la limitación GNSS de la Heltec


**Separación de datos:**
Este dashboard **no mezcla** RSSI/SNR de la Heltec con coordenadas de LINK ONE como si fueran del mismo paquete. Cada serie temporal está filtrada explícitamente por el DevEUI correspondiente. El mapa GNSS solo muestra puntos de los dispositivos que tienen coordenadas almacenadas en InfluxDB (actualmente, solo LINK ONE).

---

## Variables de Grafana

Al importar los dashboards, se deben revisar las siguientes variables:

| Variable | Valor por defecto | Descripción |
|---|---|---|
| `DS_INFLUXDB` | *(selección dinámica)* | Fuente de datos InfluxDB configurada en Grafana. Se selecciona al importar. |
| `bucket` | `lorawan-gps-uav` | Nombre del bucket de InfluxDB donde ChirpStack almacena los datos. |
| `heltec_dev_eui` | `a4cf123456789a01` | DevEUI de la Heltec Wireless Tracker (hardcoded en consultas Flux). |
| `heltec_name` | `heltec-tracker-01` | Nombre del dispositivo Heltec en ChirpStack. |
| `linkone_dev_eui` | `000000000000100d` | DevEUI de LINK ONE 13 (hardcoded en consultas Flux). |
| `linkone_name` | `LINK ONE 13` | Nombre del dispositivo LINK ONE en ChirpStack. |

> **Nota:** Los DevEUI están codificados directamente en las consultas Flux de cada panel (no como variables de plantilla de Grafana) para evitar errores de configuración y garantizar que cada panel consulta el dispositivo correcto.

---

## Configuración de codecs por dispositivo

Cada dispositivo LoRaWAN del TFG utiliza un codec diferente en ChirpStack para decodificar el payload binario:

| Dispositivo | DevEUI | Codec en ChirpStack | Archivo |
|---|---|---|---|
| **Heltec Wireless Tracker** | `a4cf123456789a01` | `Custom JavaScript codec functions` | `payload-decoders/chirpstack_decoder_20_bytes.js` |
| **LINK ONE 13** | `000000000000100d` | `CayenneLPP` (integrado en ChirpStack) | — (no requiere archivo externo) |

### Heltec — Decoder JavaScript personalizado (20 bytes)
La Heltec envía un payload binario propietario de **20 bytes** con GNSS + métricas del dron. Se debe copiar el contenido de `payload-decoders/chirpstack_decoder_20_bytes.js` en **Device Profiles → Codec → Custom JavaScript codec functions** del perfil de la Heltec.

### LINK ONE — Cayenne LPP (integrado)
LINK ONE 13 utiliza el formato estándar **Cayenne LPP** (Low Power Payload). En ChirpStack, se activa seleccionando `CayenneLPP` en el desplegable de **Device Profiles → Codec → Payload codec**. No requiere ningún código adicional. Este formato incluye latitud, longitud y altitud codificadas automáticamente, lo que explica por qué las coordenadas GNSS de LINK ONE aparecen correctamente en InfluxDB.

> ⚠️ **Importante:** Aplicar el decoder equivocado a un dispositivo causará que ChirpStack devuelva error en cada uplink y ningún dato llegue a InfluxDB. Verificar siempre el Device Profile correcto para cada DevEUI.

---

## Cómo importar los dashboards en Grafana

1. Iniciar sesión en Grafana con una cuenta con permisos de edición.
2. En el menú principal, ir a **Dashboards**.
3. Hacer clic en **New** → **Import**.
4. Hacer clic en **Upload JSON file** y seleccionar el archivo `.json` correspondiente de la carpeta `dashboards/`.
5. En el campo **InfluxDB**, seleccionar la fuente de datos InfluxDB configurada en la instancia de Grafana.
6. Verificar que la variable **bucket** tiene el valor `lorawan-gps-uav`.
7. Hacer clic en **Import**.
8. Una vez importado, ajustar el rango temporal (esquina superior derecha) al intervalo de tiempo en que se realizaron las pruebas.

---

## Interpretación de los dashboards en el TFG

### Heltec Wireless Tracker — Mota principal
La Heltec es el dispositivo central del sistema UAV-LoRaWAN. Su función es transmitir por LoRaWAN tanto los datos de posicionamiento GNSS como las métricas de telemetría del dron (batería, altitud por ToF, velocidad, tiempo de vuelo y estado del SDK). El dashboard de Heltec permite validar la comunicación LoRaWAN extremo a extremo y analizar el comportamiento del sistema en condiciones reales de vuelo.

### LINK ONE 13 — Mota de apoyo
LINK ONE se utiliza como dispositivo de apoyo para validar la visualización geográfica en Grafana. Dado que sus coordenadas GNSS se almacenan correctamente en InfluxDB, permite demostrar que el mapa de cobertura funciona correctamente cuando los datos de posición están disponibles. Esto facilita la interpretación de los resultados del TFG: si el mapa funciona para LINK ONE pero no para Heltec, el problema es específico de la cadena de procesamiento de la Heltec (payload decoder o integración), no de Grafana.

### Dashboard conjunto — Monitorización global
El dashboard conjunto permite tener una visión del sistema completo durante las sesiones de prueba, facilitando la comparación simultánea de los dos dispositivos. Es especialmente útil para detectar diferencias en la calidad del enlace radio y para verificar en tiempo real que los datos del dron se están recibiendo correctamente.

### GNSS de la Heltec en Grafana
La **ausencia de coordenadas de la Heltec en Grafana no implica que el módulo GNSS no funcione**. Los eventos de ChirpStack confirman que se reciben tramas con coordenadas válidas. La incidencia se encuentra en la cadena de persistencia ChirpStack → InfluxDB. Las posibles causas incluyen: configuración del payload decoder en ChirpStack, configuración de la integración InfluxDB en ChirpStack, o el nombre de los campos en el payload decodificado.

---

## Limitaciones conocidas

- **Coordenadas GNSS de Heltec:** Con el decoder actualizado (`chirpstack_decoder_20_bytes.js`), los campos `latitude`, `longitude` y `altitude_m` **siempre se escriben en InfluxDB** (nunca `null`). Cuando no hay fix GPS, el valor almacenado es `0`. Esto permite distinguir entre "integración rota" (panel vacío) y "sin fix GPS" (panel con valor 0 constante).

- **Valores centinela del decoder Heltec:**
  - `latitude = 0` y `longitude = 0` → sin fix GPS. Filtrar en Grafana con `r._value != 0` para excluir del mapa.
  - `altitude_m = 0` → altitud no disponible o sin fix.
  - `drone_battery_percent = 255` → sin comunicación con el dron en ese ciclo (el firmware envía -1 como `int8_t` → byte `0xFF` = 255).
  - `drone_tof_cm`, `drone_speed`, `drone_time_s = -1` → sin dato del dron en ese ciclo.
  - `gps_fix_raw = 0` → confirma que las coordenadas 0,0 son centinela, no coordenadas reales.

- **Grafana solo muestra lo que está en InfluxDB:** Si un campo no está almacenado en InfluxDB, el panel aparecerá vacío. Con el decoder actualizado, esto indica un problema en el decoder o en la integración ChirpStack→InfluxDB, no en el dispositivo.

- **No combinar datos de dispositivos distintos:** Los paneles de cada dashboard filtran estrictamente por el DevEUI correspondiente. No mezclar RSSI/SNR de un dispositivo con las coordenadas GNSS de otro.

- **Mapa GNSS de Heltec:** Para representar la cobertura radio de la Heltec sobre un mapa, se debe añadir un panel geomap filtrando `r.latitude != 0.0 and r.longitude != 0.0` para excluir los puntos sin fix GPS. Esto no está incluido en los dashboards actuales porque la Heltec aún no tiene fix GNSS confirmado en pruebas.

- **Geohash LINK ONE:** El panel de geohash de LINK ONE depende de que el payload decoder de LINK ONE incluya este campo. Si no aparece, el panel estará vacío pero no producirá errores.

- **Unidades de velocidad:** La velocidad del dron se muestra en cm/s según el formato del payload del firmware.

---

## Estructura de la carpeta `dashboards/`

```
dashboards/
├── dashboard_linkone_gnss.json                          # Dashboard LINK ONE (validación GNSS)
├── dashboard_heltec_lorawan_dron.json                   # Dashboard Heltec (mota principal + dron)
├── dashboard_conjunto_heltec_linkone_dron.json          # Dashboard conjunto
└── README.md                                            # Este archivo
```

> **Nota:** La carpeta `grafana/` (antes ubicación principal) contiene un README de redirección a esta carpeta.

---

*TFG — Diseño y evaluación de una red UAV basada en LoRaWAN para entornos de emergencia*
