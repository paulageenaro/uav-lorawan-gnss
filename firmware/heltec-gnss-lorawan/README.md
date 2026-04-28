# Firmware Heltec GNSS LoRaWAN

Este directorio contiene el código en C++/Arduino para la mota embarcada en el UAV.

## Requisitos
- **Arduino IDE** (v1.8 o v2.x).
- Paquete de placas **ESP32** instalado.
- Librería de soporte para placas **Heltec** (que incluye `LoRaWan_APP.h`).
- Librería **TinyGPS++** de Mikal Hart para decodificación NMEA.

## Configuración y Compilación

1. Abre `heltec_gnss_lorawan.ino` en Arduino IDE.
2. Duplica el archivo `config.example.h` y nómbralo `config.h`.
3. Abre `config.h` e introduce los valores reales de tu `devEui`, `appEui` y `appKey` de ChirpStack. **NOTA:** El archivo `config.h` está ignorado en Git para proteger tus credenciales.
4. Selecciona tu placa Heltec (ej. *Heltec WiFi LoRa 32 (V3)* o similar) en el menú `Herramientas > Placa`.
5. Compila y sube el código.
