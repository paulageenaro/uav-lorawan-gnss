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
    var value = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    return value;
  }

  var fix = bytes[0];
  var sats = bytes[1];

  var latRaw = readInt32BE(bytes[2], bytes[3], bytes[4], bytes[5]);
  var lonRaw = readInt32BE(bytes[6], bytes[7], bytes[8], bytes[9]);
  var altRaw = readInt16BE(bytes[10], bytes[11]);

  var droneBattery = bytes[12];
  var droneTof = readInt16BE(bytes[13], bytes[14]);
  var droneSpeed = readInt16BE(bytes[15], bytes[16]);
  var droneTime = readInt16BE(bytes[17], bytes[18]);
  var droneSdk = bytes[19];

  var gpsValid = fix === 1;
  var altValid = altRaw !== -32768;

  return {
    data: {
      gps_fix: gpsValid,
      gps_fix_raw: fix,
      satellites: sats,

      latitude: gpsValid ? latRaw / 1000000 : null,
      longitude: gpsValid ? lonRaw / 1000000 : null,
      altitude_m: altValid ? altRaw : null,

      drone_battery_percent: droneBattery,
      drone_tof_cm: droneTof,
      drone_speed: droneSpeed,
      drone_time_s: droneTime,
      drone_sdk_active: droneSdk === 1,
      drone_sdk_raw: droneSdk
    }
  };
}
