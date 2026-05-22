# Configuración en ChirpStack

Para que ChirpStack acepte y procese correctamente los paquetes provenientes de la mota Heltec instalada en el dron, se deben seguir estos pasos:

## 1. Crear el Device Profile
El *Device Profile* indica al servidor las capacidades técnicas del nodo.
1. Ir a **Device Profiles** y hacer clic en *Add device-profile*.
2. Nombre: (Ej. "Heltec LoRa32 GNSS").
3. Región: `EU868` (o la configurada en la región).
4. Versión MAC: `1.0.3` o `1.0.2` (según la librería LoRaWAN).
5. Revisión Regional Parameters: (Ej. `RP002-1.0.3`).
6. ADDR (Adaptive Data Rate): Puede activarse si la red es estable, pero para un dron en movimiento rápido, a veces se prefiere fijar el Data Rate para evitar pérdida de paquetes.
7. Pestaña **Codec**: Seleccionar `Custom JavaScript codec functions` y pegar el código de `/payload-decoders/chirpstack_decoder.js`.

## 2. Registrar el Dispositivo (Device)
1. Ir a **Applications** -> Seleccionar la aplicación -> **Devices** -> *Add device*.
2. Nombre: (Ej. "UAV-Node-1").
3. DevEUI: Introducir el DevEUI configurado en el código principal (`heltec_gnss_lorawan.ino`).
4. Seleccionar el *Device Profile* creado en el paso anterior.
5. Al guardar, en la pestaña **Keys (OTAA)**:
   - Introducir la **Application Key (AppKey)**.
   - (*Nota*: Dependiendo de la versión de LoRaWAN, puede pedir *JoinEUI*, que corresponde al `appEui`).

## 3. Verificar Conexión
1. Encender la placa Heltec.
2. Abrir la pestaña **LoRaWAN frames** del dispositivo en ChirpStack. Se deberían ver mensajes de tipo `JoinRequest` seguidos de un `JoinAccept` por parte del servidor.
3. Posteriormente, en la pestaña **Events**, se deberían ver mensajes de `up` con los datos del GNSS decodificados en la sección `object`.
