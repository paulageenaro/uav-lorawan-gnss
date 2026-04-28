# Configuración en ChirpStack

Para que ChirpStack acepte y procese correctamente los paquetes provenientes de la mota Heltec instalada en el dron, sigue estos pasos:

## 1. Crear el Device Profile
El *Device Profile* indica al servidor las capacidades técnicas del nodo.
1. Ve a **Device Profiles** y haz clic en *Add device-profile*.
2. Nombre: (Ej. "Heltec LoRa32 GNSS").
3. Región: `EU868` (o la configurada en tu región).
4. Versión MAC: `1.0.3` o `1.0.2` (según tu librería LoRaWAN).
5. Revisión Regional Parameters: (Ej. `RP002-1.0.3`).
6. ADDR (Adaptive Data Rate): Puede activarse si tu red es estable, pero para un dron en movimiento rápido, a veces se prefiere fijar el Data Rate para evitar pérdida de paquetes.
7. Pestaña **Codec**: Selecciona `Custom JavaScript codec functions` y pega el código de `/payload-decoders/chirpstack_decoder.js`.

## 2. Registrar el Dispositivo (Device)
1. Ve a **Applications** -> Selecciona tu aplicación -> **Devices** -> *Add device*.
2. Nombre: (Ej. "UAV-Node-1").
3. DevEUI: Introduce el DevEUI que configures en tu código principal (`heltec_gnss_lorawan.ino`).
4. Selecciona el *Device Profile* que creaste en el paso anterior.
5. Al guardar, en la pestaña **Keys (OTAA)**:
   - Introduce la **Application Key (AppKey)**.
   - (*Nota*: Dependiendo de la versión de LoRaWAN, puede pedirte *JoinEUI*, que corresponde a tu `appEui`).

## 3. Verificar Conexión
1. Enciende la placa Heltec.
2. Abre la pestaña **LoRaWAN frames** del dispositivo en ChirpStack. Deberías ver mensajes de tipo `JoinRequest` seguidos de un `JoinAccept` por parte del servidor.
3. Posteriormente, en la pestaña **Events**, deberías ver mensajes de `up` con los datos del GNSS decodificados en la sección `object`.
