// Decodificador para ChirpStack (Codec Custom JavaScript)
// Convierte el payload binario de 12 bytes del nodo GNSS a JSON

function decodeUplink(input) {
  var bytes = input.bytes;
  
  if (bytes.length !== 12) {
    return {
      errors: ["Tamaño de payload incorrecto. Se esperaban 12 bytes."]
    };
  }

  var lat = (bytes[2] << 24 | bytes[3] << 16 | bytes[4] << 8 | bytes[5]);
  if (lat > 0x7FFFFFFF) lat -= 0x100000000;
  
  var lon = (bytes[6] << 24 | bytes[7] << 16 | bytes[8] << 8 | bytes[9]);
  if (lon > 0x7FFFFFFF) lon -= 0x100000000;
  
  var alt = (bytes[10] << 8 | bytes[11]);
  if (alt > 0x7FFF) alt -= 0x10000;

  return {
    data: {
      fix: bytes[0] === 1,
      satellites: bytes[1],
      latitude: lat / 1000000.0,
      longitude: lon / 1000000.0,
      altitude: alt === -32768 ? null : alt
    }
  };
}
