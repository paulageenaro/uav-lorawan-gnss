# Troubleshooting (Resolución de Problemas)

## 1. La placa Heltec no se une a la red LoRaWAN (Fallo en OTAA Join)
- **Causa común:** Claves incorrectas en `config.h`.
- **Solución:** Verifica que el `devEui`, `appEui` y `appKey` coincidan exactamente con los valores en ChirpStack. Verifica también que el orden de los bytes no esté invertido (LSB vs MSB, la librería Heltec suele requerir MSB).
- **Causa común:** Cobertura de Gateway insuficiente.
- **Solución:** Asegúrate de que el Gateway esté encendido, conectado a internet y suficientemente cerca del nodo.

## 2. El Fix del GNSS se mantiene a `0` o Latitud/Longitud son `0`
- **Causa común:** El módulo GNSS no tiene visibilidad del cielo.
- **Solución:** Si estás dentro de un edificio, sal al exterior. Un módulo GNSS típico puede tardar desde unos segundos hasta un par de minutos en hacer el "Cold Start" y adquirir suficientes satélites.
- **Causa común:** Los pines RX/TX están mal conectados.
- **Solución:** Revisa que el Pin TX del ESP32 vaya al Pin RX del GNSS, y viceversa. Asegúrate también de que los baudios del GNSS sean 115200 (como se define en `GNSS.begin`).

## 3. ChirpStack muestra un error de Codec
- **Causa común:** El tamaño del payload no es 12 bytes.
- **Solución:** Verifica los logs del servidor para ver cuántos bytes se están recibiendo. Si recibes algo distinto a 12, es posible que la configuración de la librería Heltec esté añadiendo cabeceras extra o que haya un problema en el `appDataSize`.
- **Solución:** Revisa el código del Custom JavaScript Codec en el Device Profile.

## 4. Reinicios constantes del ESP32
- **Causa común:** Alimentación insuficiente.
- **Solución:** El módulo GNSS y el chip de radio LoRa pueden consumir picos de corriente importantes. Si estás alimentando por USB, asegúrate de que el cable y el puerto proporcionen suficiente corriente. Si usas batería, asegúrate de que esté cargada.
