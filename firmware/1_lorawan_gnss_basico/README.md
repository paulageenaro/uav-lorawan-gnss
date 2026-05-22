# Prueba Básica: LoRaWAN + GNSS

Esta carpeta contiene el *firmware* para la mota Heltec Wireless Tracker, encargado de la primera fase de pruebas: obtención de la posición por GNSS y su retransmisión directa por LoRaWAN.

Dentro encontrarás dos versiones de la placa:

## 1. `v1_estable`
- **Estado:** Código definitivo y estable para producción.
- **Funcionamiento:** Usa la función `LoRaWAN.sleep()` nativa de la placa. Este modo ahorra muchísima batería al desconectar periféricos entre envíos.
- **Importante:** Al usar el modo sleep, el puerto USB de la placa se apagará temporalmente entre ciclos. Es un comportamiento esperado, no un fallo. El ordenador hará el sonido de desconectar/conectar USB.

## 2. `v2_depuracion`
- **Estado:** Código para pruebas y recuperación.
- **Funcionamiento:** Exactamente igual que el estable, pero se ha reemplazado `LoRaWAN.sleep()` por retardos tradicionales (`delay()`). 
- **Importante:** Utilizar este código si se está programando o monitorizando el puerto serie (Serial Monitor) y se necesita ver las trazas continuas (como el `JoinAccept`) sin que se corte la conexión USB.

### Formato de datos (Payload)
Ambas versiones utilizan el mismo decodificador en ChirpStack (`chirpstack_decoder.js` en la carpeta `payload-decoders`) para interpretar los **12 bytes** de datos:
- `Byte 0`: GPS Fix (0/1)
- `Byte 1`: Número de Satélites
- `Bytes 2-5`: Latitud
- `Bytes 6-9`: Longitud
- `Bytes 10-11`: Altitud (m)
