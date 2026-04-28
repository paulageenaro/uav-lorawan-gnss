# Decodificadores de Payload

En ChirpStack, los nodos envían datos codificados en base64 o como matriz de bytes brutos a través del aire para ahorrar ancho de banda. El "Network Server" necesita un "Codec" o decodificador para interpretar estos bytes y convertirlos a formato JSON legible (por ejemplo, para integraciones con bases de datos o dashboards).

## Uso en ChirpStack v4

1. Abre la consola web de ChirpStack.
2. Navega a **Device Profiles** y selecciona el perfil que usan tus motas Heltec.
3. Ve a la pestaña **Codec**.
4. En **Payload codec**, selecciona `Custom JavaScript codec functions`.
5. En el campo de texto, copia y pega el contenido del archivo `chirpstack_decoder.js` de esta carpeta.
6. Guarda los cambios.

A partir de este momento, los mensajes que lleguen a ChirpStack incluirán el objeto `object` con los datos descodificados (latitud, longitud, altitud, etc.).
