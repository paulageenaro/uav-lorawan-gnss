# Troubleshooting (Resolución de Problemas)

## 1. El puerto USB se desconecta continuamente
- **Causa común:** El código de la versión estable usa `LoRaWAN.sleep()`. Este modo apaga el controlador USB para ahorrar energía máxima.
- **Solución:** No es un error. Si necesitas ver los logs por el monitor serie de forma continua para depurar, carga el código de la carpeta `v2_depuracion` en lugar de la versión estable.

## 2. La placa no transmite datos (Join exitoso pero sin uplinks)
- **Causa común:** En el entorno de Arduino, funciones bloqueantes pueden paralizar la máquina de estados de LoRaWAN.
- **Solución:** En el código se utiliza `readGpsWindow(1000)` en lugar de esperas activas infinitas para evitar este bloqueo. Asegúrate de no añadir `delay()` largos dentro de los casos `DEVICE_STATE_SEND` o `SLEEP`.

## 3. ChirpStack muestra un error de Codec
- **Causa común:** El tamaño del payload enviado no coincide con el esperado por el decodificador.
- **Solución:** Verifica que el código de la mota (12 o 20 bytes) corresponda con el script JavaScript seleccionado en el **Device Profile** de ChirpStack.

## 4. Grafana no muestra los datos, pero ChirpStack sí
- **Causa común:** Las variables en InfluxDB se guardan bajo el tag `_measurement` y no como simples campos en un measurement genérico.
- **Solución:** En Grafana, la consulta Flux debe incluir explícitamente `|> filter(fn: (r) => r["_measurement"] == "device_frmpayload_data_drone_battery_percent")` (por ejemplo) en vez de buscar en el campo genérico.

## 5. El Fix del GNSS se mantiene a `0`
- **Causa común:** El módulo GNSS no tiene visibilidad del cielo (Cold Start incompleto).
- **Solución:** Sal al exterior. Los módulos envían coordenadas a 0 o envían latitud/longitud inválidas hasta triangular al menos 3 o 4 satélites.
