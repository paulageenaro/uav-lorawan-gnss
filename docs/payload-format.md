# Formato de Payload LoRaWAN

Dado el ancho de banda restrictivo de las redes LPWAN, los datos se empaquetan en binario (codificación **Big-Endian**) en lugar de texto plano JSON.

## 1. Payload Básico (12 bytes)
Utilizado en la prueba `1_lorawan_gnss_basico`. (appPort = 2)

| Byte | Tamaño | Descripción | Formato de Compresión |
|------|--------|-------------|-----------------------|
| `0` | 1 byte | Estado del Fix | `1` si hay fix GNSS válido, `0` si es inválido |
| `1` | 1 byte | Satélites | Número entero de satélites en vista |
| `2-5` | 4 bytes | Latitud | Entero con signo de 32 bits (`int32`). Valor = `Latitud * 1,000,000` |
| `6-9` | 4 bytes | Longitud | Entero con signo de 32 bits (`int32`). Valor = `Longitud * 1,000,000` |
| `10-11`| 2 bytes | Altitud | Entero con signo de 16 bits (`int16`). En metros. Especial `-32768` si es inválida. |

## 2. Payload Integrado UAV (20 bytes)
Utilizado en la prueba `2_lorawan_gnss_uav_integrado`. Se compone de los 12 bytes del GNSS más 8 bytes adicionales de telemetría del dron.

| Byte | Tamaño | Descripción | Formato de Compresión |
|------|--------|-------------|-----------------------|
| `0-11` | 12 bytes | Posición GNSS | (Ver tabla de Payload Básico superior) |
| `12` | 1 byte | Batería Dron | Porcentaje (0-100%). Entero 8 bits (`int8`). |
| `13-14` | 2 bytes | Altímetro ToF | Centímetros. Entero 16 bits (`int16`). |
| `15-16` | 2 bytes | Velocidad Dron | Centímetros/segundo. Entero 16 bits (`int16`). |
| `17-18` | 2 bytes | Tiempo Vuelo | Segundos. Entero 16 bits (`int16`). |
| `19` | 1 byte | Estado SDK | `1` si el SDK está activo, `0` en caso contrario. |

## Decodificación (Lado del Servidor)
En ChirpStack, estos payloads deben invertirse. En la carpeta `/payload-decoders/` encontrarás `chirpstack_decoder.js` (para 12 bytes) y `chirpstack_decoder_20_bytes.js` (para 20 bytes).
