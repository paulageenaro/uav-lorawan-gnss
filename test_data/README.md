# Datos de Prueba (Test Data)

Esta carpeta está destinada a almacenar archivos de datos generados durante las pruebas del sistema UAV-LoRaWAN. Es muy útil conservar estos archivos como evidencia para el TFG y para facilitar la depuración o el rediseño de decodificadores y dashboards.

## Tipos de archivos recomendados

- **JSON de eventos de ChirpStack**: Exportaciones de la pestaña "Events" en ChirpStack, mostrando cómo llegan los paquetes `up` (uplink) con el payload decodificado y la metadata (RSSI, SNR).
- **Exportaciones de InfluxDB**: Consultas exportadas en formato CSV desde InfluxDB para validar qué datos se están persistiendo.
- **Trazas del puerto serie**: Logs del monitor serie de las motas (Heltec o pasarela ESP32) capturados durante las pruebas.

## Archivos de prueba documentados

### 1. `heltec_evento_uplink_con_gnss.json`
Exportación de la pestaña "Events" en ChirpStack.  
**Qué demuestra:** Confirma que el payload decoder de 20 bytes funciona a la perfección. El objeto JSON contiene:
- `gps_fix: true` y `satellites: 6` (GNSS validado).
- Coordenadas reales decodificadas: `latitude: 37.197222`, `longitude: -3.624661` y `altitude_m: 709`.
- Valores centinela del dron recibidos correctamente (`drone_battery_percent: 255`, resto a `-1`) indicando que el dron no estaba volando durante este uplink.
- RSSI `-50` y SNR `14`, indicando un enlace radio de altísima calidad (mota muy cercana al gateway).

### 2. `heltec_frame_bruto_20_bytes.json`
Exportación de la pestaña "LoRaWAN frames" en ChirpStack del mismo mensaje.  
**Qué demuestra:** Permite ver el payload binario bruto tal cual lo envía el firmware de la Heltec y lo lee la capa física de LoRaWAN.
- `frmPayload: "0106023795a6ffc8b12b02c5ffffffffffff00"`
- Se pueden aislar directamente los 20 bytes. Por ejemplo, los 4 bytes de latitud son `023795a6` (en decimal 37197222, que dividido entre 1.000.000 cuadra perfectamente con la latitud decodificada).
