# UAV LoRaWAN GNSS Node & Multi-Device Monitoring

Este repositorio contiene la implementación práctica y la documentación técnica de una **infraestructura de comunicaciones de emergencia basada en LoRaWAN y monitorización multi-dispositivo con UAVs (drones)**. 

El sistema permite geolocalizar y monitorizar en tiempo real múltiples motas sensoras (motos de rescate, personal en tierra o balizas) y vehículos aéreos no tripulados (UAVs) en zonas con cobertura celular nula.

---

## 🚀 Arquitectura del Prototipo

La solución admite dos escenarios de despliegue según las necesidades operativas:

### 1. Sistema Base (Geolocalización con Mota LoRaWAN-GNSS)
Permite el rastreo directo en exteriores. Una mota sensora autónoma (basada en el chip ESP32 Heltec Wireless Tracker) obtiene sus coordenadas satelitales y las emite mediante el protocolo de red LPWAN LoRaWAN.

### 2. Sistema Integrado UAV (Localizador + Telemetría Dron)
El nodo final viaja embarcado en un **UAV (Dron DJI RoboMaster TT / Tello Talent)**. Un microcontrolador **ESP32 intermedio** interroga al dron por WiFi mediante su SDK y UDP (puerto 8889). Las métricas capturadas (batería, altitud ToF, velocidad, tiempo) se retransmiten localmente por *broadcast* UDP hacia la mota Heltec. Esta unifica las lecturas del GNSS propio con la telemetría del dron en un solo paquete binario consolidado y lo transmite a la red LoRaWAN.

La infraestructura receptora en la nube se compone de:
*   **Gateway LoRaWAN**: Concentrador de RF que recibe los paquetes LoRa y los reenvía por protocolo IP.
*   **Network Server (ChirpStack v4)**: Autentica las motas (OTAA), deduplica paquetes y decodifica las tramas binarias a JSON legible utilizando decodificadores JavaScript personalizados.
*   **Base de Datos (InfluxDB)**: Registra los datos decodificados y los metadatos de radio (RSSI, SNR, fCnt) como series temporales.
*   **Visualización (Grafana)**: Presenta la telemetría histórica y en tiempo real a los operadores mediante paneles interactivos y mapas geográficos multi-capa.

---

## 📂 Estructura del Repositorio

*   [`/firmware`](./firmware/): Códigos fuente C++/Arduino para los controladores del sistema:
    *   **`1_lorawan_gnss_basico`**: Firmware básico de geolocalización autónoma. Cuenta con versión estándar con bajo consumo `v1_estable` (`LoRaWAN.sleep()`) y versión de desarrollo `v2_depuracion` (`delay()`).
    *   **`2_lorawan_gnss_uav_integrado`**: Firmware para el puente `esp32_pasarela` (WiFi-UDP al SDK del dron) y `heltec_mota_integrada` (fusión de datos y transmisión LoRa).
*   [`/dashboards`](./dashboards/): Ficheros de configuración JSON listos para importar en Grafana:
    *   `dashboard_heltec_lorawan_dron.json`: Especializado en la mota principal Heltec y la telemetría del UAV.
    *   `dashboard_linkone_gnss.json`: Individualizado para la mota de apoyo secundaria LINK ONE (Cayenne LPP).
    *   `dashboard_conjunto_heltec_linkone_dron.json`: Dashboard integrador y unificado multi-nodo con mapas cartográficos conjuntos en tiempo real.
*   [`/payload-decoders`](./payload-decoders/): Decodificadores JavaScript (`decodeUplink`) para el Network Server de ChirpStack.
*   [`/docs`](./docs/): Guías detalladas de arquitectura, formato de compresión binaria y resolución de problemas (*troubleshooting*).
*   [`/images`](./images/): Registro de diagramas de red, evidencias reales de ChirpStack, logs de tramas y capturas de los paneles de Grafana durante las pruebas de campo.
*   [`/test_data`](./test_data/): Volcados y payloads reales en formato JSON útiles para depuración de codecs y bases de datos.

---

## 📦 Formato de Tramas (Payloads)

Para maximizar la eficiencia y cumplir con el ciclo de trabajo (*Duty Cycle*) de las bandas ISM europeas (EU868), los datos viajan codificados en binario (codificación **Big-Endian**):

1.  **Payload Básico (12 bytes)**:
    `Fix (1B) | Satélites (1B) | Latitud (4B, int32) | Longitud (4B, int32) | Altitud (2B, int16)`
2.  **Payload Ampliado UAV (20 bytes)**:
    `Payload Básico (12B) | Batería UAV (1B, uint8) | ToF Altímetro (2B, int16) | Velocidad UAV (2B, int16) | Tiempo Vuelo (2B, int16) | Estado SDK (1B)`

*Nota: Ante fallas físicas o pérdida de enlace (por ejemplo, desconexión con el dron), el firmware y los decodificadores utilizan valores centinela específicos (batería a 255, ToF/velocidad a -1, coordenadas sin fix a 0) que garantizan la integridad de la base de datos sin colapsar las consultas visuales.*

---

## ⚙️ Configuración Rápida

1.  **Carga del Firmware**: Abre los ficheros `.ino` correspondientes de `/firmware/` con Arduino IDE. Configura tus credenciales OTAA (`DevEUI`, `AppEUI`, `AppKey`) y graba el código en tus dispositivos.
2.  **Configuración en ChirpStack**: Crea los perfiles de dispositivo adecuados.
    *   Para la Heltec, selecciona un codec JavaScript personalizado y pega el contenido de `payload-decoders/chirpstack_decoder_20_bytes.js`.
    *   Para el dispositivo secundario de apoyo (ej. LinkOne), puedes seleccionar el codec estándar integrado `CayenneLPP`.
3.  **Configuración de InfluxDB**: Vincula ChirpStack con tu base de datos mediante la integración HTTP de ChirpStack, apuntando al bucket configurado (ej. `lorawan-gps-uav`).
4.  **Importación en Grafana**: Ve a *Dashboards -> New -> Import* en tu instancia de Grafana y carga cualquiera de los ficheros JSON de la carpeta `/dashboards/`. Asigna tu Datasource de InfluxDB y ajusta el rango de tiempo de consulta a la sesión de tus pruebas.

---

## 📄 Licencia

Este proyecto se distribuye bajo la licencia MIT. Consulta el archivo `LICENSE` para más información.

