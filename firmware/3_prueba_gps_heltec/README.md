# Prueba de Diagnóstico GNSS (sin LoRaWAN)

Este boceto (*sketch*) de Arduino está diseñado exclusivamente para **diagnosticar y verificar el correcto funcionamiento del módulo GNSS integrado en la placa Heltec Wireless Tracker** (basada en el chip ESP32) sin necesidad de conectarse a la red LoRaWAN o registrar el dispositivo en ChirpStack.

---

## 🛠️ ¿Para qué sirve?

Al depurar problemas de geolocalización, es útil aislar el hardware para descartar:
1. Problemas de cobertura de radio LoRaWAN.
2. Bloqueos causados por las ventanas de envío o la máquina de estados OTAA.
3. Fallos lógicos en la biblioteca de comunicación.

Este código simplemente alimenta el receptor GNSS, abre la interfaz serie UART, captura las tramas NMEA brutas procedentes del satélite y las decodifica en variables comprensibles (latitud, longitud, altitud, satélites, hora UTC, etc.) empleando la biblioteca `TinyGPS++`.

---

## 🔌 Requisitos de Hardware

*   **Dispositivo**: Heltec Wireless Tracker (ESP32-S3 con GNSS UC6580).
*   **Antena**: Es indispensable conectar la antena GNSS activa en el puerto correspondiente (U.FL IPEX marcado como GNSS/GPS) antes de encender la placa para no dañar el módulo de RF y poder obtener señal.
*   **Entorno**: Para conseguir *fix* (fijación de posición), el dispositivo debe estar preferiblemente en exteriores con visión directa del cielo o muy próximo a una ventana despejada.

---

## 🚀 Instrucciones de Uso

1.  **Cargar el código**: Abre `3_prueba_gps_heltec.ino` en el Arduino IDE.
2.  **Configurar la placa**: Selecciona la placa Heltec correspondiente y el puerto COM.
3.  **Subir**: Compila y sube el firmware.
4.  **Monitorizar**: Abre el Monitor Serie de Arduino a una velocidad de **115200 baudios**.
5.  **Depuración NMEA**: Si sospechas que el módulo no se está comunicando con el ESP32, puedes descomentar la línea `Serial.write(c);` dentro de la función `readGNSS()` para imprimir por pantalla el flujo continuo de sentencias `$GNGGA`, `$GNRMC`, etc.
