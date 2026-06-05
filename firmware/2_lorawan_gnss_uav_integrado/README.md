# Firmware Integrado: LoRaWAN + GNSS + UAV

Esta carpeta contiene el firmware final para la integración directa de un dron en la red LoRaWAN mediante la mota Heltec Wireless Tracker, eliminando intermediarios físicos.

## `heltec_mota_integrada`

Este código se graba en la mota Heltec Wireless Tracker principal.

- **Función:** Se conecta directamente al punto de acceso WiFi del dron (ej. `TELLO-99454F`), interroga su SDK via comandos UDP para obtener su telemetría (batería, altitud ToF, velocidad y tiempo de vuelo), lee las coordenadas de su propio sensor GNSS, consolida los datos en un único paquete binario de 20 bytes y lo transmite a la red LoRaWAN.
- **Flujo:** En cada ciclo de transmisión, la mota da una ventana de lectura GNSS previa (`GNSS_PRE_UPLINK_READ_MS`), enciende su transceptor WiFi, se asocia con el AP del dron, abre un puerto UDP local para activar el modo SDK (`command`) y consultar de forma secuencial las métricas. Posteriormente apaga el transceptor WiFi (para evitar interferencias de radio con LoRa y reducir consumo) y construye el payload con los datos GNSS y del dron antes de iniciar el envío.

### Objetivos de diseño

1. Mantener el ciclo LoRaWAN de Heltec, incluido `LoRaWAN.sleep()`.
2. Mantener el GNSS **siempre alimentado**: NO se apaga VEXT en ningún momento del ciclo.
3. Conectarse directamente al WiFi del dron/Tello para leer métricas UDP.
4. **No enviar coordenadas falsas**: solo se envía posición real si `gps_fix = 1`.
5. Mantener compatibilidad con el codec ChirpStack de 20 bytes.

---

### Gestión del GNSS

El firmware implementa una estrategia de **GNSS siempre alimentado** para maximizar la probabilidad de mantener/adquirir fix:

| Función | Descripción |
|---|---|
| `keepGnssPowered()` | Se invoca constantemente en el loop, en los delays y durante la comunicación WiFi. Garantiza que VEXT permanezca en HIGH y el pin de reset del GNSS en HIGH. Nunca se apaga VEXT durante el ciclo normal. |
| `startGnssOnce()` | Inicializa la UART del GNSS una sola vez en el `setup()`. Llamadas posteriores solo refrescan la alimentación. |
| `feedGnss()` | Lee continuamente las tramas NMEA del buffer UART y las pasa al parser TinyGPS++. Se llama en todos los puntos de espera para no perder datos. |
| `delayWithGnss(ms)` | Sustituye a los `delay()` convencionales. Mantiene alimentado el GNSS, lee NMEA y muestra depuración durante toda la espera. |
| `hasFreshGpsFix()` | Validación estricta del fix: la posición debe ser válida, tener una edad menor que `GPS_MAX_AGE_MS` (5 s) y **no ser 0,0** (rechaza la coordenada nula como posición real). |
| `printGnssDebugIfNeeded()` | Imprime por el puerto serie cada 2 s: caracteres procesados, sentencias con fix, satélites, estado de la posición, latitud/longitud, edad y altitud. Activada por defecto (`DEBUG_GNSS = true`). |

#### Ventana de warmup en boot

Al arrancar, el firmware ejecuta una ventana de calentamiento de **30 segundos** (`GNSS_BOOT_WARMUP_MS`) en la que solo lee tramas NMEA sin hacer nada más. Esto da tiempo al receptor GNSS para obtener efemérides y adquirir su primer fix antes de que empiece el ciclo LoRaWAN.

> 💡 Si en arranque en frío tarda mucho en pillar fix, se puede aumentar `GNSS_BOOT_WARMUP_MS` a 60000 o 120000 ms.

---

### Ciclo de estados y modo sleep

La mota sigue la máquina de estados estándar de la librería LoRaWAN de Heltec:

```
INIT → JOIN (OTAA) → SEND → CYCLE → SLEEP → (despertador) → SEND → ...
```

| Estado | Qué ocurre |
|---|---|
| `DEVICE_STATE_INIT` | Inicializa la pila LoRaWAN y configura el Data Rate por defecto (DR3). |
| `DEVICE_STATE_JOIN` | Realiza el join OTAA con ChirpStack. Solo ocurre al inicio o si se pierde la sesión. |
| `DEVICE_STATE_SEND` | Llama a `prepareTxFrame()`: da una ventana GNSS previa, se conecta al WiFi del dron, realiza consultas UDP al SDK, lee GNSS, construye el payload de 20 bytes y envía el uplink. |
| `DEVICE_STATE_CYCLE` | Programa el próximo envío con `LoRaWAN.cycle(txDutyCycleTime)` y pasa a SLEEP. |
| `DEVICE_STATE_SLEEP` | Entra en el **sleep de bajo consumo de la librería** hasta que el temporizador expira. El GNSS se mantiene alimentado y leyendo durante este estado. |

En cada iteración del `loop()`, antes de evaluar la máquina de estados, se ejecutan `keepGnssPowered()`, `feedGnss()` y `printGnssDebugIfNeeded()` para que el GNSS nunca deje de ser atendido.

#### Tipo de sleep: `LoRaWAN.sleep()` ≠ Deep Sleep del ESP32

El firmware usa `LoRaWAN.sleep(loraWanClass)`, que es el **sleep gestionado por la librería LoRaWAN de Heltec**, **no** el deep sleep nativo del ESP32 (`esp_deep_sleep_start()`). Las diferencias clave son:

- **`LoRaWAN.sleep()`** — Light sleep gestionado internamente por la librería. El ESP32 reduce su frecuencia y entra en un estado de bajo consumo, pero **mantiene la RAM, la sesión LoRaWAN activa y el contexto de la máquina de estados**. No es necesario reinicializar nada al despertar. Durante este estado, el parser GNSS sigue procesando bytes disponibles en el buffer UART.
- **Deep sleep del ESP32** — Apaga casi todo el chip; al despertar se produce un reset completo, la RAM se pierde (salvo RTC memory) y habría que reiniciar la sesión LoRaWAN. No se usa en este firmware.

> ℹ️ **Implicación práctica:** El monitor serie puede desconectarse brevemente durante el sleep de la librería, lo que es normal. No indica un fallo de la mota. Al volver al estado SEND, la comunicación serie se restaura automáticamente.

---

### Consulta al dron (WiFi/UDP)

En cada ciclo de transmisión, la función `gatherTelloMetrics()` ejecuta el siguiente flujo:

1. **Reset de métricas** (`resetDroneMetricsForThisCycle()`): las métricas del dron se reinician a sus valores centinela antes de cada consulta. Esto evita arrastrar datos de ciclos anteriores.
2. **Conexión WiFi**: la mota activa WiFi en modo STA y se asocia al AP del dron (timeout 8 s). Durante la espera, sigue alimentando y leyendo el GNSS.
3. **Activación del SDK**: envía el comando `command` al dron por UDP. Si responde `ok`, el SDK está activo.
4. **Consulta secuencial**: `battery?`, `tof?`, `speed?`, `time?`, con pequeños delays entre cada comando para no saturar al dron.
5. **Cierre**: se cierra el socket UDP, se apaga **solo** WiFi (`WiFi.disconnect(true); WiFi.mode(WIFI_OFF)`) y se reafirma la alimentación del GNSS antes de continuar.

Si la conexión WiFi falla o el dron no responde, las métricas se envían con sus valores centinela (batería 255, el resto -1) y el sistema sigue funcionando con normalidad.

---

### Formato de datos (Payload)

Esta versión utiliza el decodificador de 20 bytes en ChirpStack (`chirpstack_decoder_20_bytes.js` en la carpeta `payload-decoders`) para interpretar:
- **12 bytes básicos:** GPS Fix, Satélites, Latitud, Longitud, Altitud.
- **8 bytes del dron:** Batería, Altímetro ToF, Velocidad, Tiempo de vuelo, Estado del SDK.

> ⚠️ **Importante:** Si `gps_fix = 0`, los campos de latitud y longitud se envían como 0. **NO son coordenadas reales.** En Grafana/InfluxDB se debe filtrar `gps_fix == 1` para mapas y análisis de posición.

### Valores centinela

| Campo | Centinela | Significado |
|---|---|---|
| `latitude`, `longitude` | 0 | Sin fix GPS (`gps_fix = 0`) |
| `altitude_m` | -32768 | Altitud GNSS no disponible |
| `drone_battery_percent` | 255 | Sin comunicación con el dron |
| `drone_tof_cm` | -1 | Sin dato ToF del dron |
| `drone_speed` | -1 | Sin dato de velocidad del dron |
| `drone_time_s` | -1 | Sin dato de tiempo de vuelo |

---

### Parámetros configurables (`heltec_mota_integrada.ino`)

#### LoRaWAN y dron

| Parámetro | Valor actual | Descripción |
|---|---|---|
| `appTxDutyCycle` | **10 000 ms (10 s)** | Intervalo entre uplinks LoRaWAN. Subir a 30 000 para dar más margen al GPS. |
| `TELLO_SSID` | `"TELLO-99454F"` | Nombre de la red Wi-Fi emitida por el dron |
| `TELLO_PASS` | `""` | Contraseña del Wi-Fi del dron (vacía por defecto) |
| `loraWanAdr` | `true` | ADR (Adaptive Data Rate) activado |
| `isTxConfirmed` | `false` | Uplinks no confirmados (unconfirmed) |
| `appPort` | `2` | Puerto FPort del uplink LoRaWAN |

#### GNSS

| Parámetro | Valor actual | Descripción |
|---|---|---|
| `GNSS_BOOT_WARMUP_MS` | **30 000 ms (30 s)** | Ventana inicial de lectura GNSS en el boot, antes de iniciar LoRaWAN |
| `GNSS_PRE_UPLINK_READ_MS` | **5 000 ms (5 s)** | Ventana de lectura GNSS antes de cada uplink |
| `GPS_MAX_AGE_MS` | **5 000 ms (5 s)** | Edad máxima aceptable de una posición para considerarla válida |
| `GNSS_DEBUG_INTERVAL_MS` | **2 000 ms (2 s)** | Frecuencia de impresión de depuración GNSS por puerto serie |
| `DEBUG_GNSS` | `true` | Activa/desactiva la impresión periódica del estado GNSS |

> ⚠️ **Nota sobre el duty cycle LoRaWAN:** Con `appTxDutyCycle = 10 000 ms` (10 s), la mota envía un uplink cada ~10 s. En la región EU868 (Europa), el duty cycle máximo por canal es del 1 %, lo que permite transmisiones cortas con suficiente margen. Sin embargo, si se usa SF alto (SF12, BW125), el tiempo en el aire de cada trama puede superar varios segundos y reducir el margen disponible. Con ADR activado, ChirpStack ajustará automáticamente el SF para optimizar el enlace.
>
> Para pruebas en laboratorio o campo abierto con pocos paquetes, 10 s es un intervalo adecuado. Para despliegues prolongados o con restricciones de batería, se recomienda aumentar a 30–60 s.

