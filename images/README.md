# Evidencias del Sistema (Imágenes)

Esta carpeta contiene capturas de pantalla que sirven como evidencia visual del correcto funcionamiento del sistema UAV-LoRaWAN para la memoria del TFG.

## Capturas de validación GNSS (Heltec)

- **`chirpstack_test_gnss_events.png`**: Captura de la pestaña "Events" de ChirpStack. Demuestra de forma definitiva que la mota Heltec consigue obtener un fix GNSS válido y que el payload decoder de 20 bytes procesa correctamente las coordenadas (latitud: 37.197222, longitud: -3.624661, altitud: 709m) junto con los valores centinela del dron.
- **`chirpstack_test_gnss_frames.png`**: Captura de la pestaña "LoRaWAN frames" de ChirpStack del mismo mensaje. Muestra el payload binario bruto (`frm_payload: "0106023795a6ffc8b12b02c5ffffffffffff00"`) de 20 bytes viajando por la red LoRaWAN en la capa física.

## Otras capturas

- **`chirpstack_events_uplink_payload.jpg`**: Captura genérica anterior de la pestaña Events mostrando la recepción de un paquete.
- **`grafana_dashboard_telemetry.jpg`**: Captura del dashboard de Grafana en funcionamiento mostrando la evolución temporal de las métricas.
