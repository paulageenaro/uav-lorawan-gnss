// Decodificador para ChirpStack v4 (Custom JavaScript Codec)
// Payload Heltec Wireless Tracker — 20 bytes
//
// Estructura del payload (big-endian):
//   byte  0      → gps_fix_raw: 0 = sin fix, 1 = fix GNSS válido
//   byte  1      → satellites: nº de satélites visibles
//   bytes 2-5    → latitude × 1e6 (int32 BE) — 0 si sin fix
//   bytes 6-9    → longitude × 1e6 (int32 BE) — 0 si sin fix
//   bytes 10-11  → altitude_m (int16 BE) — 0 si no disponible
//   byte  12     → drone_battery_percent (0-100; 255 = sin dato de dron)
//   bytes 13-14  → drone_tof_cm (int16 BE; -1 = sin dato de dron)
//   bytes 15-16  → drone_speed (int16 BE; -1 = sin dato de dron)
//   bytes 17-18  → drone_time_s (int16 BE; -1 = sin dato de dron)
//   byte  19     → drone_sdk_raw: 0 = SDK inactivo/sin dato, 1 = SDK activo
//
// VALORES CENTINELA — importantes para Grafana/InfluxDB:
//   latitude = 0 y longitude = 0  → sin fix GNSS (filtrar en Grafana: r._value != 0)
//   altitude_m = 0                → altitud no disponible (ídem)
//   drone_battery_percent = 255   → sin comunicación con el dron
//   drone_tof_cm, drone_speed, drone_time_s = -1 → sin dato del dron
//
// NOTA: lat/lon/alt_m SIEMPRE devuelven un número (nunca null).
// Esto garantiza que ChirpStack los escribe en InfluxDB aunque no haya fix GPS,
// lo que permite diagnosticar la cadena de integración desde Grafana.

function decodeUplink(input) {
  var bytes = input.bytes;

  if (bytes.length !== 20) {
    return {
      errors: ["Payload inválido: se esperaban 20 bytes y se han recibido " + bytes.length]
    };
  }

  function readInt16BE(b0, b1) {
    var value = (b0 << 8) | b1;
    if (value & 0x8000) {
      value = value - 0x10000;
    }
    return value;
  }

  function readInt32BE(b0, b1, b2, b3) {
    // Desplazamiento con signo para int32
    var value = ((b0 << 24) | (b1 << 16) | (b2 << 8) | b3);
    return value;
  }

  var fix  = bytes[0];
  var sats = bytes[1];

  var latRaw = readInt32BE(bytes[2],  bytes[3],  bytes[4],  bytes[5]);
  var lonRaw = readInt32BE(bytes[6],  bytes[7],  bytes[8],  bytes[9]);
  var altRaw = readInt16BE(bytes[10], bytes[11]);

  var droneBattery = bytes[12];              // 0-100 normal; 255 = sin dato
  var droneTof     = readInt16BE(bytes[13], bytes[14]);  // cm; -1 = sin dato
  var droneSpeed   = readInt16BE(bytes[15], bytes[16]);  // cm/s; -1 = sin dato
  var droneTime    = readInt16BE(bytes[17], bytes[18]);  // s; -1 = sin dato
  var droneSdk     = bytes[19];              // 0 o 1

  var gpsValid = (fix === 1);
  var altValid = (altRaw !== -32768);

  return {
    data: {
      // --- GNSS ---
      // gps_fix = 1 indica fix válido; latitude y longitude son 0 si sin fix.
      // Grafana: filtrar r._value != 0 para excluir puntos sin fix del mapa/stat.
      gps_fix_raw:  fix,
      gps_fix:      gpsValid ? 1 : 0,
      satellites:   sats,
      latitude:     gpsValid ? (latRaw / 1000000) : 0,
      longitude:    gpsValid ? (lonRaw / 1000000) : 0,
      altitude_m:   altValid ? altRaw : 0,

      // --- Métricas del dron ---
      // drone_battery_percent = 255 → sin comunicación con el dron (firmware envía -1 como int8 → 0xFF).
      // drone_tof_cm, drone_speed, drone_time_s = -1 → sin dato del dron en este ciclo.
      drone_battery_percent: droneBattery,
      drone_tof_cm:          droneTof,
      drone_speed:           droneSpeed,
      drone_time_s:          droneTime,
      drone_sdk_raw:         droneSdk,
      drone_sdk_active:      (droneSdk === 1) ? 1 : 0
    }
  };
}
