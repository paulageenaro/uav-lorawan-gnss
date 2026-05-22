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

### Ciclo de estados y modo sleep

La mota sigue la máquina de estados estándar de la librería LoRaWAN de Heltec:

```
INIT → JOIN (OTAA) → SEND → CYCLE → SLEEP → (despertador) → SEND → ...
```

| Estado | Qué ocurre |
|---|---|
| `DEVICE_STATE_INIT` | Inicializa la pila LoRaWAN y configura el Data Rate por defecto (DR3). |
| `DEVICE_STATE_JOIN` | Realiza el join OTAA con ChirpStack. Solo ocurre al inicio o si se pierde la sesión. |
| `DEVICE_STATE_SEND` | Llama a `prepareTxFrame()`: escucha UDP del dron (5 s), lee GNSS, construye el payload de 20 bytes y envía el uplink. |
| `DEVICE_STATE_CYCLE` | Programa el próximo envío con `LoRaWAN.cycle(txDutyCycleTime)` y pasa a SLEEP. |
| `DEVICE_STATE_SLEEP` | Entra en el **sleep gestionado por la librería** hasta que el temporizador del ciclo expira. |

#### Tipo de sleep: `LoRaWAN.sleep()` ≠ Deep Sleep del ESP32

El firmware usa `LoRaWAN.sleep(loraWanClass)`, que es el **sleep gestionado por la librería LoRaWAN de Heltec**, **no** el deep sleep nativo del ESP32 (`esp_deep_sleep_start()`). Las diferencias clave son:

- **`LoRaWAN.sleep()`** — Light sleep gestionado internamente por la librería. El ESP32 reduce su frecuencia y entra en un estado de bajo consumo, pero **mantiene la RAM, la sesión LoRaWAN activa y el contexto de la máquina de estados**. No es necesario reinicializar nada al despertar. Durante este estado, el parser GNSS sigue procesando bytes disponibles en el buffer UART.
- **Deep sleep del ESP32** — Apaga casi todo el chip; al despertar se produce un reset completo, la RAM se pierde (salvo RTC memory) y habría que reiniciar la sesión LoRaWAN. No se usa en este firmware.

> ℹ️ **Implicación práctica:** El monitor serie puede desconectarse brevemente durante el sleep de la librería, lo que es normal. No indica un fallo de la mota. Al volver al estado SEND, la comunicación serie se restaura automáticamente.

### Formato de datos (Payload)
Esta versión necesita el decodificador de 20 bytes en ChirpStack (`chirpstack_decoder_20_bytes.js` en la carpeta `payload-decoders`) para interpretar:
- **12 bytes básicos:** GPS Fix, Satélites, Latitud, Longitud, Altitud.
- **8 bytes del dron:** Batería, Altímetro ToF, Velocidad, Tiempo de vuelo, Estado del SDK.

### Parámetros configurables (`heltec_mota_integrada.ino`)

| Parámetro | Valor actual | Descripción |
|---|---|---|
| `appTxDutyCycle` | **10 000 ms (10 s)** | Intervalo entre uplinks LoRaWAN |
| `UDP_LISTEN_WINDOW_MS` | 5 000 ms (5 s) | Ventana de escucha WiFi/UDP al dron antes de cada uplink |
| `loraWanAdr` | `true` | ADR (Adaptive Data Rate) activado |
| `isTxConfirmed` | `false` | Uplinks no confirmados (unconfirmed) |
| `appPort` | `2` | Puerto FPort del uplink LoRaWAN |

> ⚠️ **Nota sobre el duty cycle LoRaWAN:** Con `appTxDutyCycle = 10 000 ms` (10 s), la mota envía un uplink cada ~10 s. En la región EU868 (Europa), el duty cycle máximo por canal es del 1 %, lo que permite transmisiones cortas con suficiente margen. Sin embargo, si se usa SF alto (SF12, BW125), el tiempo en el aire de cada trama puede superar varios segundos y reducir el margen disponible. Con ADR activado, ChirpStack ajustará automáticamente el SF para optimizar el enlace.
>
> Para pruebas en laboratorio o campo abierto con pocos paquetes, 10 s es un intervalo adecuado. Para despliegues prolongados o con restricciones de batería, se recomienda aumentar a 30–60 s.

