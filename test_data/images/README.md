# Evidencias del Sistema (Imágenes y Logs)

Esta carpeta contiene capturas de pantalla y archivos de registro que sirven como evidencia visual del correcto funcionamiento del sistema UAV-LoRaWAN.

---

## 1. Capturas de validación GNSS y Payload (Heltec)

*   **`chirpstack_test_gnss_events.png`**: Captura de la pestaña "Events" de ChirpStack. Demuestra de forma definitiva que la mota Heltec consigue obtener un fix GNSS válido en exteriores y que el payload decoder de 20 bytes procesa correctamente las coordenadas (latitud: 37.197222, longitud: -3.624661, altitud: 709m) junto con los valores centinela del dron.
*   **`chirpstack_test_gnss_frames.png`**: Captura de la pestaña "LoRaWAN frames" de ChirpStack para el mismo mensaje. Muestra el payload binario bruto (`frm_payload: "0106023795a6ffc8b12b02c5ffffffffffff00"`) de 20 bytes viajando por la red LoRaWAN en la capa física.
*   **`chirpstack_events_uplink_payload.jpg`**: Captura genérica de la interfaz de ChirpStack detallando la estructura de un evento de enlace ascendente (uplink).
*   **`chirpstack_uplink_log.json`**: Registro completo en formato JSON exportado directamente de los eventos de ChirpStack. Muestra una trama de telemetría real enviada por el dispositivo `heltec-tracker-01` en SF7 a 867.1 MHz el **23 de mayo de 2026**. Contiene los metadatos de radio (RSSI de -106 dBm, SNR de -7 dB), fix de GPS válido (1) con 11 satélites y la activación de los valores centinela de error (batería a 255 y sensores a -1) que comprueban la tolerancia a fallos del decodificador ante la desconexión física del dron.

---

## 2. Paneles de Visualización en Grafana (Sesión 23/05/2026)

Estas capturas corresponden a las pruebas de campo en tiempo real utilizando la infraestructura Grafana + InfluxDB en el host local `192.168.137.2`:

### A. Dashboard de Mota Heltec + Dron
*   **`grafana_heltec_uav_dashboard_1144.png`**
*   **`grafana_heltec_uav_dashboard_1253.png`**
    *   *Descripción:* Cuadro de mando exclusivo para la monitorización de la mota principal Heltec instalada en el dron. Visualiza la telemetría del vuelo, los satélites GPS visibles y la calidad de la señal de radio. El contraste entre ambas capturas tomadas a distintas horas (11:44 y 12:53) demuestra la estabilidad temporal del sistema de visualización de series temporales durante una sesión de vuelo real.

### B. Dashboard Individual de Mota LinkOne
*   **`grafana_linkone_individual_dashboard.png`**
    *   *Descripción:* Panel de control individualizado para el nodo `LinkOne 13` (mota secundaria o tracker alternativo). Muestra de forma aislada su cobertura LoRaWAN (RSSI/SNR), el contador de tramas (`fCnt`) y su trayectoria GPS con el fin de auditar su rendimiento por separado.

### C. Dashboard Conjunto Multi-Nodo (Integración Completa)
*   **`grafana_conjunto_dashboard_1252.png`**
*   **`grafana_conjunto_dashboard_1255.png`**
    *   *Descripción:* Cuadro de mando integrador y definitivo del proyecto ("TFG Conjunto - Heltec, LinkOne, Dron"). Combina en una única interfaz cartográfica y de control las trayectorias de ambos localizadores de emergencia (Heltec y LinkOne) junto con las métricas del dron. Permite contrastar de manera interactiva la cobertura de ambos dispositivos físicos y el estado de la aeronave en tiempo real.

---

## 3. Captura Histórica de Telemetría

*   **`grafana_dashboard_telemetry.jpg`**: Captura original del primer prototipo del dashboard de Grafana, mostrando la evolución histórica de las métricas de red y telemetría de una única mota.

