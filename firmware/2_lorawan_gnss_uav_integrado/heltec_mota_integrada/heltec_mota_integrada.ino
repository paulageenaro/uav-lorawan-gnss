#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "HT_TinyGPS++.h"
#include "esp_system.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// ====================================================
// OBJETIVO 
// ====================================================
// 1) Mantener el ciclo LoRaWAN de Heltec, incluido LoRaWAN.sleep().
// 2) Mantener el GNSS alimentado: NO se apaga VEXT en cada ciclo.
// 3) Conectarse directamente al WiFi del dron/Tello para leer métricas UDP.
// 4) No enviar coordenadas falsas: solo se envía posición si gps_fix = 1.
// 5) Mantener compatibilidad con el codec ChirpStack de 20 bytes.

// ====================================================
// GNSS
// ====================================================
HardwareSerial GNSS(1);
TinyGPSPlus gps;

// Pines GNSS
static const int VEXT_CTRL_PIN = 3;    // Alimentación GNSS
static const int GNSS_RX_PIN   = 33;   // ESP32 RX <- GNSS TX
static const int GNSS_TX_PIN   = 34;   // ESP32 TX -> GNSS RX
static const int GNSS_RST_PIN  = 35;   // Reset GNSS

// VEXT en HIGH enciende el módulo.
static const uint8_t GNSS_VEXT_ON_LEVEL = HIGH;

// Tiempos GNSS.
// Si al arrancar en frío tarda mucho en pillar fix, sube GNSS_BOOT_WARMUP_MS a 60000 o 120000.
static const uint32_t GNSS_BOOT_WARMUP_MS     = 30000;  // ventana inicial para que el GNSS empiece a recibir NMEA
static const uint32_t GNSS_PRE_UPLINK_READ_MS = 5000;   // lectura GNSS antes de cada uplink
static const uint32_t GPS_MAX_AGE_MS          = 5000;   // una posición más vieja que esto NO se acepta
static const uint32_t GNSS_DEBUG_INTERVAL_MS  = 2000;

static const bool DEBUG_GNSS = true;

unsigned long lastGnssDebugPrint = 0;
bool gnssStarted = false;

// ====================================================
// WiFi/UDP directo con Tello / RoboMaster TT
// ====================================================
const char* TELLO_SSID = "TELLO-99454F";  // SSID del dron
const char* TELLO_PASS = "";              // Tello normalmente no tiene contraseña

IPAddress TELLO_IP(192, 168, 10, 1);
const uint16_t TELLO_CMD_PORT   = 8889;
const uint16_t LOCAL_TELLO_PORT = 8889;

WiFiUDP telloUdp;

// ====================================================
// OTAA keys - deben coincidir con ChirpStack
// ====================================================
uint8_t devEui[] = {
  0xA4, 0xCF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0x01
};

uint8_t appEui[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01
};

uint8_t appKey[] = {
  0x11, 0x22, 0x33, 0x44,
  0x55, 0x66, 0x77, 0x88,
  0x99, 0xAA, 0xBB, 0xCC,
  0xDD, 0xEE, 0xF0, 0x01
};

// ABP no usado, pero la librería Heltec espera estas variables.
uint8_t nwkSKey[] = { 0 };
uint8_t appSKey[] = { 0 };
uint32_t devAddr = 0;

// ====================================================
// Configuración LoRaWAN
// ====================================================
uint16_t userChannelsMask[6] = {
  0x00FF, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000
};

LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
DeviceClass_t loraWanClass = CLASS_A;

uint32_t appTxDutyCycle = 10000;   // 10 s entre envíos. Subirlo a 30000 para dar más margen al GPS.
bool overTheAirActivation = true;  // OTAA
bool loraWanAdr = true;            // ADR activado
bool isTxConfirmed = false;        // uplinks no confirmados
uint8_t appPort = 2;
uint8_t confirmedNbTrials = 4;

// ====================================================
// Métricas del dron
// Valores centinela:
//   batería = 255 si no hay dato
//   tof/speed/time = -1 si no hay dato
//   sdk = 0 si no hay SDK o no hay comunicación
// Internamente usamos -1 y al empaquetar convertimos batería a 255.
// ====================================================
int droneBattery = -1;
int droneTof     = -1;
int droneSpeed   = -1;
int droneTime    = -1;
int droneSdk     = 0;
int droneWifiRssi = 0;

// ====================================================
// GNSS: alimentación, lectura y depuración
// ====================================================
static void keepGnssPowered() {
  // Esto se llama muchas veces para evitar que otro bloque deje VEXT mal.
  // Importante: NO se llama nunca a VEXT LOW en el ciclo normal.
  pinMode(VEXT_CTRL_PIN, OUTPUT);
  digitalWrite(VEXT_CTRL_PIN, GNSS_VEXT_ON_LEVEL);

  pinMode(GNSS_RST_PIN, OUTPUT);
  digitalWrite(GNSS_RST_PIN, HIGH);
}

static void startGnssOnce() {
  if (gnssStarted) {
    keepGnssPowered();
    return;
  }

  keepGnssPowered();
  delay(500);

  GNSS.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
  gnssStarted = true;

  Serial.println("GNSS iniciado con el mismo patrón que el sketch de prueba");
  Serial.println("VEXT=HIGH, RST=HIGH, UART1=115200, RX=33, TX=34");
}

static void feedGnss() {
  // Lectura continua de tramas NMEA.
  while (GNSS.available()) {
    char c = GNSS.read();
    gps.encode(c);

    // Descomenta esto para ver NMEA en bruto:
    // Serial.write(c);
  }
}

static void printGnssDebugIfNeeded() {
  if (!DEBUG_GNSS) {
    return;
  }

  if (millis() - lastGnssDebugPrint < GNSS_DEBUG_INTERVAL_MS) {
    return;
  }
  lastGnssDebugPrint = millis();

  Serial.println();
  Serial.println("========== ESTADO GNSS ==========");
  Serial.print("Caracteres recibidos del GNSS: ");
  Serial.println(gps.charsProcessed());

  Serial.print("Sentencias con fix: ");
  Serial.println(gps.sentencesWithFix());

  Serial.print("Satélites detectados: ");
  if (gps.satellites.isValid()) {
    Serial.println(gps.satellites.value());
  } else {
    Serial.println("sin dato");
  }

  Serial.print("Estado posición: ");
  if (gps.location.isValid()) {
    Serial.println("POSICIÓN NMEA VÁLIDA");
    Serial.print("Latitud recibida: ");
    Serial.println(gps.location.lat(), 6);
    Serial.print("Longitud recibida: ");
    Serial.println(gps.location.lng(), 6);
    Serial.print("Edad de la posición (ms): ");
    Serial.println(gps.location.age());
  } else {
    Serial.println("SIN FIX TODAVÍA");
    Serial.println("Todavía no hay solución GNSS válida.");
  }

  Serial.print("Altitud válida?: ");
  Serial.println(gps.altitude.isValid() ? "SI" : "NO");
  if (gps.altitude.isValid()) {
    Serial.print("Altitud recibida (m): ");
    Serial.println(gps.altitude.meters());
  }

  Serial.println("=================================");
}

static void delayWithGnss(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    keepGnssPowered();
    feedGnss();
    printGnssDebugIfNeeded();
    delay(2);
    yield();
  }
}

static bool hasFreshGpsFix() {
  if (!gps.location.isValid()) {
    return false;
  }

  if (gps.location.age() > GPS_MAX_AGE_MS) {
    return false;
  }

  // No aceptar 0,0 como posición real.
  if (gps.location.lat() == 0.0 && gps.location.lng() == 0.0) {
    return false;
  }

  return true;
}

static uint8_t getSatelliteCount() {
  if (!gps.satellites.isValid()) {
    return 0;
  }

  uint32_t sats = gps.satellites.value();
  if (sats > 255) {
    sats = 255;
  }
  return (uint8_t)sats;
}

// ====================================================
// WiFi/UDP Tello
// ====================================================
static void resetDroneMetricsForThisCycle() {
  // No se arrastran métricas antiguas de ciclos anteriores.
  droneBattery = -1;
  droneTof     = -1;
  droneSpeed   = -1;
  droneTime    = -1;
  droneSdk     = 0;
  droneWifiRssi = 0;
}

String sendTelloCommandSafe(const String& cmd, uint32_t timeoutMs = 1500) {
  // Limpiar UDP pendiente.
  while (telloUdp.parsePacket() > 0) {
    while (telloUdp.available()) {
      telloUdp.read();
    }
  }

  Serial.print("[TELLO TX] ");
  Serial.println(cmd);

  telloUdp.beginPacket(TELLO_IP, TELLO_CMD_PORT);
  telloUdp.print(cmd);
  telloUdp.endPacket();

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    keepGnssPowered();
    feedGnss();

    int packetSize = telloUdp.parsePacket();
    if (packetSize > 0) {
      char buffer[128];
      int len = telloUdp.read(buffer, sizeof(buffer) - 1);
      if (len > 0) {
        buffer[len] = '\0';
      } else {
        buffer[0] = '\0';
      }

      String response = String(buffer);
      response.trim();

      Serial.print("[TELLO RX] ");
      Serial.println(response);
      return response;
    }

    delay(5);
    yield();
  }

  Serial.print("[TELLO RX] TIMEOUT: ");
  Serial.println(cmd);
  return "";
}

int responseToIntOrUnavailable(const String& response) {
  if (response.length() == 0) {
    return -1;
  }
  if (response == "ok" || response == "error") {
    return -1;
  }
  return response.toInt();
}

void gatherTelloMetrics() {
  resetDroneMetricsForThisCycle();

  Serial.println();
  Serial.println("----- WIFI/UDP TELLO -----");
  Serial.print("Conectando al WiFi del dron: ");
  Serial.println(TELLO_SSID);

  keepGnssPowered();
  WiFi.mode(WIFI_STA);
  WiFi.begin(TELLO_SSID, TELLO_PASS);

  uint32_t startConnect = millis();
  uint32_t lastDot = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startConnect < 8000) {
    keepGnssPowered();
    feedGnss();

    if (millis() - lastDot > 500) {
      Serial.print(".");
      lastDot = millis();
    }
    delay(10);
    yield();
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No se pudo conectar al WiFi del dron.");
    Serial.println("Se enviarán métricas del dron como SIN DATO, pero GNSS/LoRaWAN siguen funcionando.");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delayWithGnss(300);
    return;
  }

  Serial.println("Conectado al WiFi del dron.");
  Serial.print("IP Heltec: ");
  Serial.println(WiFi.localIP());

  droneWifiRssi = WiFi.RSSI();
  Serial.print("RSSI WiFi: ");
  Serial.println(droneWifiRssi);

  if (!telloUdp.begin(LOCAL_TELLO_PORT)) {
    Serial.println("No se pudo iniciar UDP local.");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delayWithGnss(300);
    return;
  }

  String sdkResp = sendTelloCommandSafe("command", 2000);
  droneSdk = (sdkResp == "ok") ? 1 : 0;

  if (droneSdk == 1) {
    droneBattery = responseToIntOrUnavailable(sendTelloCommandSafe("battery?", 1500));
    delayWithGnss(80);

    droneTof = responseToIntOrUnavailable(sendTelloCommandSafe("tof?", 1500));
    delayWithGnss(80);

    droneSpeed = responseToIntOrUnavailable(sendTelloCommandSafe("speed?", 1500));
    delayWithGnss(80);

    droneTime = responseToIntOrUnavailable(sendTelloCommandSafe("time?", 1500));
    delayWithGnss(80);
  } else {
    Serial.println("SDK no activo o sin respuesta. Métricas del dron = SIN DATO.");
  }

  telloUdp.stop();

  Serial.println("Apagando SOLO WiFi antes de transmitir por LoRaWAN. El GNSS se mantiene alimentado.");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  keepGnssPowered();
  delayWithGnss(300);
}

// ====================================================
// Empaquetado big-endian compatible con tu codec de 20 bytes
// ====================================================
static void putInt32BE(uint8_t *buf, int32_t v) {
  buf[0] = (uint8_t)((v >> 24) & 0xFF);
  buf[1] = (uint8_t)((v >> 16) & 0xFF);
  buf[2] = (uint8_t)((v >> 8) & 0xFF);
  buf[3] = (uint8_t)(v & 0xFF);
}

static void putInt16BE(uint8_t *buf, int16_t v) {
  buf[0] = (uint8_t)((v >> 8) & 0xFF);
  buf[1] = (uint8_t)(v & 0xFF);
}

// ====================================================
// Payload ChirpStack de 20 bytes
// byte 0      -> gps_fix: 0 sin fix, 1 fix válido
// byte 1      -> satellites
// bytes 2-5   -> latitude * 1e6, solo real si gps_fix = 1
// bytes 6-9   -> longitude * 1e6, solo real si gps_fix = 1
// bytes 10-11 -> altitude_m. Si no hay altitud válida: -32768
// byte 12     -> batería dron: 0-100, 255 sin dato
// bytes 13-14 -> tof cm, -1 sin dato
// bytes 15-16 -> speed, -1 sin dato
// bytes 17-18 -> time s, -1 sin dato
// byte 19     -> SDK activo: 0/1
// ====================================================
static void prepareTxFrame(uint8_t port) {
  (void)port;

  // 1) Antes de consultar el dron, damos tiempo al GNSS.
  delayWithGnss(GNSS_PRE_UPLINK_READ_MS);

  // 2) Consulta directa al dron por WiFi/UDP.
  // Si falla, no bloquea el sistema: las métricas se mandan como sin dato.
  gatherTelloMetrics();

  // 3) Margen final para procesar NMEA antes de decidir si hay fix.
  delayWithGnss(1000);

  bool gpsValid = hasFreshGpsFix();

  uint8_t fix = gpsValid ? 1 : 0;
  uint8_t sats = getSatelliteCount();

  int32_t lat = 0;
  int32_t lon = 0;
  int16_t alt = -32768;

  if (gpsValid) {
    lat = (int32_t)(gps.location.lat() * 1000000.0);
    lon = (int32_t)(gps.location.lng() * 1000000.0);

    if (gps.altitude.isValid()) {
      alt = (int16_t)gps.altitude.meters();
    }
  }

  // MUY IMPORTANTE:
  // Si gpsValid = false, lat/lon se quedan en 0 
  // NO son coordenadas reales. El campo que manda es gps_fix = 0.
  // En Grafana/InfluxDB se debe filtrar gps_fix == 1 para mapas y análisis de posición.

  appDataSize = 20;
  appData[0] = fix;
  appData[1] = sats;
  putInt32BE(&appData[2], lat);
  putInt32BE(&appData[6], lon);
  putInt16BE(&appData[10], alt);

  appData[12] = (droneBattery >= 0 && droneBattery <= 100) ? (uint8_t)droneBattery : (uint8_t)255;
  putInt16BE(&appData[13], (droneTof   >= 0) ? (int16_t)droneTof   : (int16_t)-1);
  putInt16BE(&appData[15], (droneSpeed >= 0) ? (int16_t)droneSpeed : (int16_t)-1);
  putInt16BE(&appData[17], (droneTime  >= 0) ? (int16_t)droneTime  : (int16_t)-1);
  appData[19] = (droneSdk == 1) ? 1 : 0;

  Serial.println();
  Serial.println("----- UPLINK -----");
  Serial.print("GPS fix válido: ");
  Serial.println(gpsValid ? "SI" : "NO");
  Serial.print("gps_fix enviado: ");
  Serial.println(fix);
  Serial.print("Satélites detectados: ");
  Serial.println(sats);

  if (gpsValid) {
    Serial.println("Estado posición: FIX VÁLIDO. Se envían coordenadas reales.");
    Serial.print("Latitud real: ");
    Serial.println(gps.location.lat(), 6);
    Serial.print("Longitud real: ");
    Serial.println(gps.location.lng(), 6);
    Serial.print("Edad posición (ms): ");
    Serial.println(gps.location.age());
  } else {
    Serial.println("Estado posición: SIN FIX. No se envían coordenadas reales.");
  }

  if (gpsValid && gps.altitude.isValid()) {
    Serial.print("Altitud real (m): ");
    Serial.println(gps.altitude.meters());
  } else {
    Serial.println("Altitud no disponible.");
  }

  Serial.println("Métricas dron del ciclo actual:");
  Serial.print("BAT: "); Serial.println(droneBattery >= 0 ? String(droneBattery) : "SIN DATO -> se envía 255");
  Serial.print("TOF: "); Serial.println(droneTof >= 0 ? String(droneTof) : "SIN DATO -> se envía -1");
  Serial.print("SPD: "); Serial.println(droneSpeed >= 0 ? String(droneSpeed) : "SIN DATO -> se envía -1");
  Serial.print("TIME: "); Serial.println(droneTime >= 0 ? String(droneTime) : "SIN DATO -> se envía -1");
  Serial.print("SDK: "); Serial.println(droneSdk);
  Serial.print("RSSI WiFi dron: "); Serial.println(droneWifiRssi);

  Serial.print("Payload bytes HEX: ");
  for (int i = 0; i < appDataSize; i++) {
    if (appData[i] < 16) {
      Serial.print("0");
    }
    Serial.print(appData[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// ====================================================
// SETUP
// ====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Inicio Heltec GNSS + WiFi/UDP dron directo + LoRaWAN");
  Serial.print("Motivo de reinicio: ");
  Serial.println(esp_reset_reason());

  startGnssOnce();

  // Arrancar con WiFi apagado. Solo se activa al consultar el dron.
  WiFi.mode(WIFI_OFF);
  delayWithGnss(300);

  Serial.println("Ventana inicial GNSS: el módulo se mantiene encendido y leyendo NMEA...");
  delayWithGnss(GNSS_BOOT_WARMUP_MS);

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  Serial.println("MCU Heltec inicializada");
}

// ====================================================
// LOOP - Máquina de estados LoRaWAN Heltec
// ====================================================
void loop() {
  keepGnssPowered();
  feedGnss();
  printGnssDebugIfNeeded();

  switch (deviceState) {
    case DEVICE_STATE_INIT:
#if (LORAWAN_DEVEUI_AUTO)
      LoRaWAN.generateDeveuiByChipID();
#endif
      LoRaWAN.init(loraWanClass, loraWanRegion);
      LoRaWAN.setDefaultDR(3);
      Serial.println("LoRaWAN inicializado");
      break;

    case DEVICE_STATE_JOIN:
      Serial.println("Intentando OTAA join...");
      LoRaWAN.join();
      break;

    case DEVICE_STATE_SEND:
      prepareTxFrame(appPort);
      keepGnssPowered();
      LoRaWAN.send();
      Serial.println("Uplink enviado a LoRaWAN");
      deviceState = DEVICE_STATE_CYCLE;
      break;

    case DEVICE_STATE_CYCLE:
      txDutyCycleTime = appTxDutyCycle + randr(-APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND);
      Serial.print("Programando siguiente envío en ");
      Serial.print(txDutyCycleTime / 1000);
      Serial.println(" segundos...");
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;

    case DEVICE_STATE_SLEEP:
      // Se mantiene el sleep propio de LoRaWAN, pero NO se apaga el GNSS desde el firmware.
      // Si la librería/placa conserva VEXT durante este sleep, el GNSS tendrá más facilidad para mantener/adquirir fix.
      keepGnssPowered();
      feedGnss();
      LoRaWAN.sleep(loraWanClass);
      break;

    default:
      deviceState = DEVICE_STATE_INIT;
      break;
  }
}