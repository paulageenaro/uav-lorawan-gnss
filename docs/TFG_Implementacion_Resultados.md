# Capítulo X: Implementación y Resultados

## 1. Descripción general del sistema implementado

El sistema desarrollado constituye una infraestructura de comunicaciones basada en LoRaWAN, orientada a la monitorización y seguimiento en escenarios de emergencia. Su función principal consiste en recopilar la ubicación y el estado de un dispositivo desplegado en campo y transmitir esta información hacia un servidor centralizado para su almacenamiento y visualización en tiempo real. 

Para validar la viabilidad y modularidad del sistema, la implementación práctica se ha dividido en dos escenarios incrementales:
1. **Sistema base (Prueba principal LoRaWAN/GNSS):** Evaluación de la conectividad básica empleando una mota sensora equipada con posicionamiento GNSS.
2. **Sistema integrado (Prueba secundaria UAV-mota):** Ampliación del sistema base mediante la integración de un Vehículo Aéreo No Tripulado (UAV), recopilando métricas de vuelo y transmitiéndolas a través de la mota hacia la red LoRaWAN.

## 2. Arquitectura de la solución sin dron

En este escenario, el nodo extremo de la red es una placa de desarrollo **Heltec Wireless Tracker / Heltec LoRaWAN-GNSS**. Esta placa se encarga de adquirir su posición geográfica a través de su receptor GNSS integrado y construir un paquete de datos (*payload*). 

La arquitectura se compone de los siguientes elementos en topología de estrella:
- **Mota Sensora (End-Device):** La placa Heltec obtiene las coordenadas y las encapsula.
- **Gateway LoRaWAN:** Actúa como puente de radiofrecuencia, recibiendo las modulaciones LoRa y reenviándolas hacia Internet mediante protocolo IP.
- **Network Server (ChirpStack):** Gestiona la red LoRaWAN, autentica la mota mediante OTAA (*Over-The-Air Activation*), deduplica paquetes y decodifica el *payload* utilizando un *codec* JavaScript.
- **Base de Datos (InfluxDB):** Almacena los paquetes decodificados como series temporales para su posterior consulta.
- **Visualización (Grafana):** Extrae la información de InfluxDB y la representa en cuadros de mando (*dashboards*), permitiendo monitorizar parámetros de red (RSSI, SNR, fCnt) y variables físicas (latitud, longitud, altitud y satélites visibles).

## 3. Arquitectura de la solución con dron

Esta arquitectura expande el caso anterior incorporando un **dron DJI RoboMaster TT / Tello Talent** y un microcontrolador **ESP32** (o kit de expansión) que actúa como pasarela WiFi-UDP.

Los componentes adicionales y su interacción son:
- **Dron (UAV):** Expone un punto de acceso (AP) WiFi y un servidor UDP (puerto 8889) a través de su SDK, permitiendo la lectura de sus métricas internas (batería, altímetro ToF, velocidad, tiempo de vuelo).
- **ESP32 (Puente UAV-Mota):** Se conecta simultáneamente a la red WiFi del dron (modo *Station*) y despliega una red inalámbrica propia (modo *Access Point*). Interroga al dron por UDP para extraer las métricas y las retransmite por *broadcast* UDP hacia la mota.
- **Mota Heltec (Nodo Integrador):** Se conecta a la red WiFi del ESP32. Durante una ventana de tiempo predefinida, escucha los paquetes UDP con las métricas del dron. Posteriormente, lee su propio GNSS, concatena toda la información (UAV + GNSS) en un único *payload* y lo transmite por la red LoRaWAN hacia el gateway.

El resto de la infraestructura (ChirpStack, InfluxDB, Grafana) se mantiene análoga al primer escenario, adaptando únicamente el *codec* y las consultas para procesar las nuevas métricas.

## 4. Flujo de datos completo

### 4.1. Flujo sin dron (Sensor/Mota)
1. **Adquisición:** La mota Heltec extrae del GNSS (`gps.encode()`) la latitud, longitud, altitud y satélites.
2. **Transmisión de Radio:** Los datos se formatean en un arreglo de bytes y se envían vía LoRaWAN (`LoRaWAN.send()`).
3. **Recepción:** El gateway LoRaWAN capta la señal y la envía al Network Server.
4. **Procesamiento:** ChirpStack recibe el paquete `UnconfirmedDataUp`, extrae los metadatos de radio (RSSI, SNR, fCnt) y decodifica el *payload*.
5. **Persistencia:** ChirpStack envía un evento a InfluxDB, escribiendo cada campo decodificado.
6. **Visualización:** Grafana ejecuta consultas sobre InfluxDB y refresca los paneles en pantalla.

### 4.2. Flujo con dron integrado (Dron/ESP32)
1. **Adquisición UAV:** El dron expone sus métricas por WiFi.
2. **Puente local:** El ESP32 envía comandos (ej. `battery?`) al dron por UDP (8889), recibe respuestas y construye una cadena de texto (ej. `BAT=87;TOF=40...`).
3. **Transmisión WiFi:** El ESP32 difunde esta cadena por *broadcast* UDP (puerto 4210).
4. **Fusión de datos:** La Heltec, conectada al WiFi del ESP32, lee este paquete UDP (`udp.parsePacket()`). A continuación, lee el GNSS.
5. **Transmisión LoRaWAN:** La Heltec construye un paquete ampliado y lo transmite por LoRaWAN, apagando temporalmente su módulo WiFi para ahorrar energía y evitar interferencias.
6. **Procesamiento en Nube:** El flujo en el Gateway, ChirpStack, InfluxDB y Grafana es idéntico, decodificando ahora las métricas adicionales (batería dron, velocidad, etc.).

## 5. Explicación de los códigos de la implementación

El *software* desarrollado en el marco del proyecto se clasifica en cuatro códigos principales, ordenados funcionalmente:

1. **Código de recuperación/depuración (`heltec_gnss_lorawan_v2.ino`):**
   * **Función:** Este código implementa el envío LoRaWAN y la lectura GNSS, pero elude el modo de suspensión profunda (`LoRaWAN.sleep()`), reemplazándolo por retardos (`delay()`). 
   * **Propósito:** Se utiliza en fases de prueba debido a que el modo *Deep Sleep* apaga el controlador USB, provocando la desconexión del monitor serie en el ordenador. Es una versión inestable para producción pero necesaria para visualizar trazas de depuración de forma continua.

2. **Código LoRaWAN/GNSS estable (`heltec_gnss_lorawan.ino`):**
   * **Función:** Es la versión definitiva para el primer escenario. Mantiene intacta la máquina de estados nativa de la librería Heltec (`DEVICE_STATE_INIT`, `JOIN`, `SEND`, `CYCLE`, `SLEEP`).
   * **Propósito:** Maximiza la eficiencia energética utilizando `LoRaWAN.sleep()`. La desconexión del puerto serie bajo esta versión es el comportamiento esperado y no un fallo del sistema. 

3. **Código del ESP32/Kit del dron (`ESP32_kit_dron.ino`):**
   * **Función:** Implementa un modo dual de red WiFi (`WIFI_AP_STA`). Por un lado, se conecta como cliente al dron Tello y gestiona el protocolo SDK mediante intercambio asíncrono UDP. Por otro lado, crea un *Access Point* y difunde las métricas parseadas.
   * **Propósito:** Desacoplar la interacción con el SDK propietario del dron de la lógica de transmisión LoRaWAN, otorgando modularidad al diseño.

4. **Código Heltec integrado (`heltec_dron_14_05.ino`):**
   * **Función:** Combina las lecturas GNSS y la recepción WiFi/UDP en la misma máquina de estados LoRaWAN. 
   * **Propósito:** Se conecta al ESP32 temporalmente en cada ciclo, recupera la cadena de datos, cierra la conexión WiFi (para no interrumpir las temporizaciones de radio y ahorrar batería), prepara el *payload* de 20 bytes y lo emite. Incorpora mecanismos de robustez: si el dron no responde, el *payload* se envía con valores predeterminados de error (`-1`).

## 6. Explicación de las tramas de datos (*Payloads*)

Para minimizar el consumo de ancho de banda y cumplir con las regulaciones de ciclo de trabajo (*Duty Cycle*) de LoRaWAN, los datos no se envían en texto plano (como JSON), sino empaquetados a nivel de bit y byte (*Big-Endian*).

### Payload Básico (12 bytes)
Utilizado en el sistema sin dron:
- `Byte 0`: Fix GPS (0 = Invalido, 1 = Válido)
- `Byte 1`: Número de satélites
- `Bytes 2-5`: Latitud (multiplicada por $10^6$, entero de 32 bits)
- `Bytes 6-9`: Longitud (multiplicada por $10^6$, entero de 32 bits)
- `Bytes 10-11`: Altitud en metros (entero de 16 bits con signo)

### Payload Ampliado (20 bytes)
Utilizado en la prueba con el UAV. Añade las métricas obtenidas por el ESP32:
- `Byte 12`: Nivel de batería del dron (%) (entero de 8 bits con signo)
- `Bytes 13-14`: Altímetro ToF del dron (cm) (entero de 16 bits)
- `Bytes 15-16`: Velocidad del dron (cm/s) (entero de 16 bits)
- `Bytes 17-18`: Tiempo de vuelo (s) (entero de 16 bits)
- `Byte 19`: Estado del SDK (0 = inactivo, 1 = activo)

## 7. Decodificación en ChirpStack (*Codec*)

El registro de los dispositivos en el Network Server se realiza bajo el procedimiento OTAA (*Over-The-Air Activation*). Este flujo inicia con un `JoinRequest` de la mota, al cual el servidor responde con un `JoinAccept` utilizando las credenciales precompartidas (`DevEUI`, `AppEUI` y `AppKey`). Una vez autenticada, la mota transmite tramas de datos `UnconfirmedDataUp`.

Para interpretar el arreglo de bytes recibido en estos *uplinks*, ChirpStack emplea un *Codec Custom JavaScript* en su configuración. Cabe destacar que el *codec* no interviene durante el proceso de *Join*, sino exclusivamente en la interpretación de las tramas de datos (`UnconfirmedDataUp`).

Para la prueba final integrada con el UAV, se diseñó un codec específico capaz de decodificar el *payload* ampliado de 20 bytes. Esta función (`decodeUplink`) realiza las siguientes operaciones clave:
1. **Validación:** Comprueba que la longitud del paquete sea exactamente 20 bytes; de lo contrario, devuelve un error ("Payload inválido...").
2. **Extracción GNSS (Bytes 0-11):** Lee el `fix` (byte 0), extrae latitud y longitud (bytes 2-9) aplicando desplazamientos de bits (`bitshift`) y dividiendo entre $10^6$ para recuperar los grados decimales, siempre que el *fix* sea válido.
3. **Extracción UAV (Bytes 12-19):** Extrae directamente la batería del dron (`drone_battery_percent`) y aplica funciones matemáticas para componer las variables de dos bytes: el altímetro (`drone_tof_cm`), la velocidad (`drone_speed`) y el tiempo de vuelo (`drone_time_s`).
4. **Formato JSON:** Genera un objeto estructurado con las siguientes claves finales:
   - `gps_fix` (booleano) y `gps_fix_raw` (0 o 1).
   - `satellites` (número de satélites).
   - `latitude`, `longitude` y `altitude_m` (valores geográficos o nulos si no hay *fix*).
   - `drone_battery_percent`, `drone_tof_cm`, `drone_speed`, `drone_time_s` (métricas de vuelo del UAV).
   - `drone_sdk_active` (booleano) y `drone_sdk_raw` (indicador del SDK del dron).

Este objeto JSON estandarizado es el que finalmente se transfiere e inyecta en el bus de datos hacia InfluxDB.

## 8. Integración con InfluxDB

El enlace entre ChirpStack e InfluxDB (Base de Datos de Series Temporales) persiste tanto los metadatos de radiofrecuencia como la información extraída del *payload*. 

- **Metadatos de red:** Los valores como `RSSI` (Indicador de fuerza de señal), `SNR` (Relación señal-ruido) y `fCnt` (Contador de tramas) son proporcionados por el Gateway y la capa MAC de LoRaWAN, por lo que se registran independientemente del contenido del *payload*.
- **Datos de aplicación:** Los campos provenientes del *codec* (`latitude`, `drone_battery_percent`, etc.) son generados en el Network Server. 

InfluxDB almacena esta información bajo una estructura específica. ChirpStack crea un `_measurement` por cada variable decodificada, con el prefijo `device_frmpayload_data_` (ej. `device_frmpayload_data_drone_battery_percent`), guardando el número dentro del campo `_value`. Este nivel de detalle estructural es fundamental para que la herramienta de visualización pueda localizar las métricas de forma independiente.

## 9. Visualización en Grafana

El panel de control (*dashboard*) diseñado en Grafana centraliza la telemetría. Este se divide en secciones lógicas:
1. **Calidad de Señal:** Gráficas temporales de `RSSI` y `SNR`, esenciales para auditar la cobertura y penetración de la tecnología LoRa.
2. **Integridad de Red:** Monitorización del contador `fCnt`. Un crecimiento lineal asegura la correcta recepción; saltos o estancamientos evidencian pérdidas de paquetes o reinicios del nodo.
3. **Telemetría UAV:** Paneles de tipo *Gauge* y gráficas temporales que muestran el nivel de batería, velocidad instantánea, altitud relativa (ToF) y tiempo de vuelo.
4. **Posicionamiento y Cobertura:** Un mapa interactivo ubica cada transmisión. El panel cruza las coordenadas geográficas con la intensidad de la señal (`RSSI`). El sistema está configurado de modo que, si el GNSS no ha logrado triangular la posición (`latitude` = nulo), no se plotea el punto erróneo, manteniendo la pureza cartográfica del mapa de cobertura.

## 10. Problemas encontrados y soluciones implementadas

Durante la etapa de integración se solventaron múltiples dificultades técnicas:

- **Desconexión USB por `LoRaWAN.sleep()`:** El modo de ahorro de energía suspendía los periféricos de la placa, provocando que el ordenador cerrase el puerto serie. Se identificó que no constituía un fallo lógico, y se diseñó el código de depuración `v2` exento de suspensiones para aislar problemas de desarrollo sin perder los registros impresos por pantalla.
- **Tráfico `JoinRequest` sin `UnconfirmedDataUp`:** En ocasiones, el nodo completaba la negociación OTAA pero no transmitía los datos. Se identificó que la rutina de lectura del GNSS bloqueaba el ciclo de ejecución. La solución consistió en implementar una lectura basada en ventanas temporales (`readGpsWindow`) asíncronas.
- **Invisibilidad de variables en Grafana:** A pesar de que ChirpStack registraba las métricas, Grafana mostraba paneles vacíos. El problema residía en el diseño de las consultas (*queries*). Se corrigió filtrando explícitamente por el tag de `_measurement` (`device_frmpayload_data_...`) y no solo intentando buscar el `_field`.
- **Interferencia WiFi/LoRa:** Activar simultáneamente la antena LoRa y la antena WiFi/Bluetooth en el procesador principal (ESP32) causaba consumos pico y colapsos de memoria. La arquitectura final resuelve esto encendiendo el WiFi únicamente en la ventana de escucha (`receiveDroneMetricsWindow`), apagándolo inmediatamente antes de solicitar el `LoRaWAN.send()`.

## 11. Resultados obtenidos

La implementación demuestra la viabilidad de utilizar LoRaWAN en escenarios de emergencia donde la conectividad convencional es nula. 

- **Comunicaciones:** Se verificó la correcta transmisión OTAA y una recepción estable en el Gateway. Las métricas de `RSSI` y `SNR` reflejan los límites del margen de enlace según el entorno (línea de vista frente a obstáculos urbanos).
- **Decodificación:** Las tramas binarias se transformaron en variables físicas congruentes en la infraestructura de la nube, validando la solidez de la codificación y de las temporizaciones de las ventanas de escucha asíncronas.
- **Limitaciones operativas:** Se evidenció que la carencia de *fix* GPS provoca el envío de la variable a nulo; el *payload* se transmite igualmente permitiendo analizar la cobertura LoRaWAN, aunque se pierda el posicionamiento. De la misma forma, si el dron pierde la conexión de la red local con la mota, la mota continúa alertando sobre su estado emitiendo banderas de error (valores de `-1`).

---

## Recomendaciones para la redacción final de la memoria

> **Sugerencia de estructura para la memoria:** 
> - **Capítulo 5. Implementación:** Aquí deben incluirse los apartados 1 al 9, priorizando los diagramas de arquitectura, fragmentos del *payload* e integración ESP32.
> - **Capítulo 6. Pruebas y Resultados:** Aquí encajan los apartados 10 y 11, adjuntando la captura del Dashboard (figura de Grafana), métricas de RSSI, trazas del *JoinAccept* y análisis de la desconexión del modo sleep.

### Recomendación de Figuras y Tablas
1. **Diagrama de Bloques / Arquitectura:** Crear una figura visual mostrando el flujo: UAV $\rightarrow$ ESP32 $\rightarrow$ Heltec $\rightarrow$ Gateway $\rightarrow$ ChirpStack $\rightarrow$ InfluxDB $\rightarrow$ Grafana.
2. **Tabla del Payload:** Trasladar la explicación del "Payload Ampliado (20 bytes)" a una tabla formal en LaTeX indicando Byte, Tipo de Dato, Rango y Función.
3. **Captura del Dashboard:** Incluir la captura `Grafana.png` en los Resultados, destacando cómo el RSSI decae según se aleja el dron y cómo la telemetría se recibe en tiempo real.
4. **Capturas de ChirpStack:** Mostrar una figura con el flujo `JoinRequest` $\rightarrow$ `JoinAccept` $\rightarrow$ `UnconfirmedDataUp`.

### 14. Trabajo futuro
Como extensión del proyecto para la sección de "Trabajo Futuro / Conclusiones", se pueden contemplar los siguientes puntos:
1. Implementación de una cola FIFO en la placa Heltec para almacenar mediciones del UAV si el envío LoRaWAN falla (pérdida de cobertura).
2. Transición del enlace ESP32 $\leftrightarrow$ Heltec de WiFi UDP a un protocolo de radio corto más eficiente como BLE (Bluetooth Low Energy).
3. Estudio del uso de ADR (*Adaptive Data Rate*) en vuelo para que el dron regule su *Spreading Factor* conforme varía su altura y se incrementa el SNR.
