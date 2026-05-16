# Prueba Integrada: LoRaWAN + GNSS + UAV

Esta carpeta contiene los dos códigos necesarios para la segunda fase del proyecto: la integración de un dron en la red LoRaWAN a través de un enlace intermedio WiFi/UDP.

## 1. `esp32_pasarela`
Este código se graba en un microcontrolador ESP32 genérico (o en el kit de expansión del dron).
- **Función:** Actúa como traductor o pasarela.
- Se conecta a la red WiFi que emite el propio dron (ej. `TELLO-99454F`).
- Envía comandos por UDP al puerto del SDK del dron (8889) para extraer: batería, altura ToF, velocidad y tiempo.
- A la vez, emite su propia red WiFi (`UAV_METRICS_AP`) y retransmite las métricas del dron por puerto UDP (4210).

## 2. `heltec_mota_integrada`
Este código se graba en la mota Heltec principal.
- **Función:** Integra todo en un único paquete y lo manda a Internet.
- **Flujo:** En cada ciclo, la mota enciende brevemente su antena WiFi, se conecta a `UAV_METRICS_AP` y escucha los datos. Acto seguido, apaga el WiFi (para no interrumpir la radio y ahorrar batería), lee su propio GNSS, concatena la información del dron y de los satélites, y hace el envío final por LoRaWAN.

### Formato de datos (Payload)
Esta versión necesita el decodificador de 20 bytes en ChirpStack (`chirpstack_decoder_20_bytes.js` en la carpeta `payload-decoders`) para interpretar:
- **12 bytes básicos:** GPS Fix, Satélites, Latitud, Longitud, Altitud.
- **8 bytes del dron:** Batería, Altímetro ToF, Velocidad, Tiempo de vuelo, Estado del SDK.
