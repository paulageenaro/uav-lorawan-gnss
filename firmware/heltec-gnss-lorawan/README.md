# Firmware Heltec GNSS LoRaWAN

Este directorio contiene el código en C++/Arduino para la mota embarcada en el UAV.

## Requisitos
- **Arduino IDE** (v1.8 o v2.x).
- Paquete de placas **ESP32** instalado.
- Librería de soporte para placas **Heltec** (que incluye `LoRaWan_APP.h`).
- Librería **TinyGPS++** de Mikal Hart para decodificación NMEA.

## Configuración y Compilación

1. Abre `heltec_gnss_lorawan.ino` en Arduino IDE.
2. Introduce los valores reales de tu `devEui`, `appEui` y `appKey` de ChirpStack directamente en el código.
3. Selecciona tu placa Heltec (ej. *Heltec WiFi LoRa 32 (V3)* o similar) en el menú `Herramientas > Placa`.
4. Compila y sube el código.
