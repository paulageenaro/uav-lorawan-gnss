# Formato de Payload LoRaWAN

Dado el ancho de banda restrictivo de las redes LPWAN (Low Power Wide Area Network) como LoRaWAN, los datos no deben enviarse en texto plano (como cadenas JSON o NMEA puras), sino que deben empaquetarse en binario.

## Estructura (Uplink)

El dispositivo envía paquetes en el puerto `2` (`appPort = 2`).  
Longitud total del payload: **12 bytes**.  
Codificación: **Big-Endian**.

| Byte | Tamaño | Descripción | Formato de Compresión |
|------|--------|-------------|-----------------------|
| `0` | 1 byte | Estado del Fix | `1` si hay fix GNSS válido, `0` si es inválido |
| `1` | 1 byte | Satélites | Número entero de satélites en vista |
| `2-5` | 4 bytes | Latitud | Entero con signo de 32 bits (`int32`). Valor = `Latitud * 1,000,000` |
| `6-9` | 4 bytes | Longitud | Entero con signo de 32 bits (`int32`). Valor = `Longitud * 1,000,000` |
| `10-11`| 2 bytes | Altitud | Entero con signo de 16 bits (`int16`). En metros. Valor especial `-32768` si no es válida. |

## Decodificación (Lado del Servidor)

En el Network Server (ChirpStack), este payload debe invertirse (dividir latitud/longitud por 1,000,000). Consulta la carpeta `/payload-decoders/` para encontrar el script en JavaScript correspondiente.
