# UAV LoRaWAN GNSS Node

Este repositorio contiene la implementación práctica del Trabajo Fin de Grado centrado en el **diseño y evaluación de una red UAV basada en LoRaWAN para entornos de emergencia**.

El objetivo de este proyecto es demostrar la viabilidad de transportar un nodo final autónomo (mota) con un Vehículo Aéreo No Tripulado (UAV/dron), capturar su ubicación mediante un módulo GNSS y enviarla periódicamente utilizando tecnología LoRaWAN.

## 🚀 Arquitectura del Prototipo

El sistema propuesto está compuesto por:
1. **Nodo Final (Mota)**: Placa basada en ESP32 (serie Heltec) conectada a un módulo GNSS. Va embarcada en el UAV.
2. **Gateway LoRaWAN**: Puerta de enlace encargada de recibir las tramas del dron y reenviarlas al Network Server.
3. **Network Server (ChirpStack)**: Plataforma encargada de la gestión del dispositivo, decodificación del payload y enrutamiento de los datos.

## 📂 Estructura del Repositorio

- [`/firmware`](./firmware/): Código fuente en C++/Arduino para el ESP32. Las claves OTAA se configuran directamente en el archivo principal.
- [`/payload-decoders`](./payload-decoders/): Scripts en JavaScript para decodificar las tramas en ChirpStack.
- [`/docs`](./docs/): Documentación detallada sobre la arquitectura, el formato de datos y la configuración del servidor.
- [`/images`](./images/): Diagramas e imágenes relacionadas con el proyecto.

## ⚙️ Uso Básico

1. Clona este repositorio.
2. Ve a la carpeta `firmware/heltec-gnss-lorawan/` y abre `heltec_gnss_lorawan.ino` con Arduino IDE.
3. Introduce tus claves LoRaWAN (DevEUI, AppEUI/JoinEUI, AppKey) directamente en el código.
4. Compila y sube el código a tu placa ESP32 mediante Arduino IDE.
5. Añade el decodificador de `/payload-decoders` a tu *Device Profile* en ChirpStack.

## 📄 Licencia

Este proyecto se distribuye bajo la licencia MIT. Consulta el archivo `LICENSE` para más información.
