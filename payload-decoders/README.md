# Decodificadores de Payload

En ChirpStack, los nodos envían datos codificados en binario a través del aire para ahorrar ancho de banda. El servidor de red necesita un codec o decodificador para interpretar esos bytes y convertirlos a JSON legible (para integraciones con InfluxDB, Grafana, etc.).

---

## Codec por dispositivo

| Dispositivo | DevEUI | Codec en ChirpStack | Archivo |
|---|---|---|---|
| **Heltec Wireless Tracker** | `a4cf123456789a01` | `Custom JavaScript codec functions` | `chirpstack_decoder_20_bytes.js` |
| **LINK ONE 13** | `000000000000100d` | `CayenneLPP` (integrado en ChirpStack) | — (no requiere archivo externo) |

---

## LINK ONE 13 — Codec Cayenne LPP (integrado en ChirpStack)

LINK ONE 13 utiliza el formato estándar **Cayenne LPP** (Low Power Payload). En ChirpStack v4, se activa sin necesidad de ningún código adicional:

1. Abre la consola web de ChirpStack.
2. Navega a **Device Profiles** y selecciona el perfil asignado a LINK ONE.
3. Ve a la pestaña **Codec**.
4. En **Payload codec**, selecciona `CayenneLPP` en el desplegable.
5. Guarda los cambios.

Cayenne LPP es un formato abierto que codifica automáticamente latitud, longitud, altitud, temperatura y otros sensores estándar. Esto explica por qué las coordenadas GNSS de LINK ONE aparecen correctamente en InfluxDB sin decoder personalizado.

---

## Heltec Wireless Tracker — Codec JavaScript personalizado (20 bytes)

La Heltec Wireless Tracker envía un payload binario **propietario de 20 bytes** con GNSS + métricas del dron. Requiere el decoder JavaScript incluido en este repositorio:

### Decoders disponibles

| Archivo | Payload | Uso |
|---|---|---|
| `chirpstack_decoder.js` | 12 bytes | Prueba GNSS básica (solo posición) |
| `chirpstack_decoder_20_bytes.js` | 20 bytes | **Prueba integrada UAV** (GNSS + métricas dron) ← usar este |

### Aplicar el decoder en ChirpStack v4

1. Abre la consola web de ChirpStack.
2. Navega a **Device Profiles** y selecciona el perfil asignado a la Heltec.
3. Ve a la pestaña **Codec**.
4. En **Payload codec**, selecciona `Custom JavaScript codec functions`.
5. Copia y pega el contenido de `chirpstack_decoder_20_bytes.js` en el editor.
6. Guarda los cambios.

### Campos decodificados (20 bytes)

| Campo JSON | Tipo | Descripción | Centinela |
|---|---|---|---|
| `gps_fix_raw` | int | 0 = sin fix, 1 = fix válido | — |
| `gps_fix` | int (0/1) | Alias de gps_fix_raw como entero | — |
| `satellites` | int | Nº de satélites visibles | — |
| `latitude` | float | Latitud en grados decimales | **0** = sin fix |
| `longitude` | float | Longitud en grados decimales | **0** = sin fix |
| `altitude_m` | int | Altitud en metros | **0** = no disponible |
| `drone_battery_percent` | int | Batería del dron (%) | **255** = sin dato |
| `drone_tof_cm` | int | Distancia ToF en cm | **-1** = sin dato |
| `drone_speed` | int | Velocidad del dron (cm/s) | **-1** = sin dato |
| `drone_time_s` | int | Tiempo de vuelo en segundos | **-1** = sin dato |
| `drone_sdk_raw` | int | Estado SDK: 0 = inactivo, 1 = activo | — |
| `drone_sdk_active` | int (0/1) | Alias booleano de drone_sdk_raw | — |

> ⚠️ **Importante sobre valores centinela:** `latitude = 0` y `longitude = 0` significan **sin fix GPS**, no coordenadas reales. En Grafana, filtrar con `r._value != 0` para excluir estas lecturas de mapas y estadísticas. El campo `gps_fix_raw` permite confirmar el motivo: si es 0, las coordenadas son centinela.

> ⚠️ **Aplicar el decoder equivocado** (por ejemplo, el de 12 bytes en una Heltec que envía 20) causará que ChirpStack devuelva un error en cada uplink y ningún dato llegará a InfluxDB.

---

*TFG — Diseño y evaluación de una red UAV basada en LoRaWAN para entornos de emergencia*
