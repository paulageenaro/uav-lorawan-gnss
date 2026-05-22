#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "HT_TinyGPS++.h"
#include "esp_system.h"
#include <WiFi.h>
#include <WiFiUdp.h>

HardwareSerial GNSS(1);
TinyGPSPlus gps;

// -------------------- Pines GNSS --------------------
static const int VEXT_CTRL_PIN = 3;
static const int GNSS_RX_PIN   = 33;
static const int GNSS_TX_PIN   = 34;
static const int GNSS_RST_PIN  = 35;

// -------------------- WiFi/UDP ESP32 dron --------------------
const char* UAV_AP_SSID = "UAV_METRICS_AP";
const char* UAV_AP_PASS = "12345678";

WiFiUDP udp;
const uint16_t UDP_LISTEN_PORT = 4210;

// Tiempo máximo escuchando métricas UDP antes de cada uplink
const uint32_t UDP_LISTEN_WINDOW_MS = 5000;

// -------------------- OTAA keys --------------------
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

// -------------------- ABP no usado --------------------
uint8_t nwkSKey[] = { 0 };
uint8_t appSKey[] = { 0 };
uint32_t devAddr = 0;

// -------------------- Configuración LoRaWAN --------------------
uint16_t userChannelsMask[6] = {
  0x00FF, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000
};

LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
DeviceClass_t loraWanClass = CLASS_A;

uint32_t appTxDutyCycle = 10000;   // 10 s entre envíos
bool overTheAirActivation = true;  // OTAA
bool loraWanAdr = true;            // ADR activado
bool isTxConfirmed = false;        // uplinks no confirmados
uint8_t appPort = 2;
uint8_t confirmedNbTrials = 4;

// -------------------- Métricas recibidas del ESP32 del dron --------------------
// Valores -1 indican que todavía no se ha recibido dato válido por UDP.
int droneBattery = -1;
int droneTof     = -1;
int droneSpeed   = -1;
int droneTime    = -1;
int droneSdk     = 0;
int droneWifiRssi = 0;

unsigned long lastUdpPacketMs = 0;

// ----------------------------------------------------
// Obtener valor de un mensaje tipo:
// BAT=87;TOF=40;SPD=10;TIME=0;SDK=1;RSSI=-45
// ----------------------------------------------------
int getValueFromMessage(String msg, const String& key, int defaultValue) {
  String search = key + "=";
  int start = msg.indexOf(search);

  if (start < 0) {
    return defaultValue;
  }

  start += search.length();

  int end = msg.indexOf(";", start);
  if (end < 0) {
    end = msg.length();
  }

  String value = msg.substring(start, end);
  value.trim();

  return value.toInt();
}

// ----------------------------------------------------
// Lee un paquete UDP si está disponible
// ----------------------------------------------------
void readUdpPacketIfAvailable() {
  int packetSize = udp.parsePacket();

  if (packetSize <= 0) {
    return;
  }

  char buffer[256];
  int len = udp.read(buffer, sizeof(buffer) - 1);

  if (len <= 0) {
    return;
  }

  buffer[len] = '\0';

  String msg = String(buffer);
  msg.trim();

  Serial.print("[UDP RX] ");
  Serial.println(msg);

  droneBattery  = getValueFromMessage(msg, "BAT", droneBattery);
  droneTof      = getValueFromMessage(msg, "TOF", droneTof);
  droneSpeed    = getValueFromMessage(msg, "SPD", droneSpeed);
  droneTime     = getValueFromMessage(msg, "TIME", droneTime);
  droneSdk      = getValueFromMessage(msg, "SDK", droneSdk);
  droneWifiRssi = getValueFromMessage(msg, "RSSI", droneWifiRssi);

  lastUdpPacketMs = millis();

  Serial.println("Métricas del dron actualizadas:");
  Serial.print("BAT = "); Serial.println(droneBattery);
  Serial.print("TOF = "); Serial.println(droneTof);
  Serial.print("SPD = "); Serial.println(droneSpeed);
  Serial.print("TIME = "); Serial.println(droneTime);
  Serial.print("SDK = "); Serial.println(droneSdk);
  Serial.print("RSSI = "); Serial.println(droneWifiRssi);
}

// ----------------------------------------------------
// Conectar al AP del ESP32 del dron y escuchar UDP
// ----------------------------------------------------
void receiveDroneMetricsWindow(uint32_t listenMs) {
  Serial.println();
  Serial.println("----- WIFI/UDP DRON -----");
  Serial.print("Conectando a AP: ");
  Serial.println(UAV_AP_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(UAV_AP_SSID, UAV_AP_PASS);

  uint32_t startConnect = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startConnect < 8000) {
    while (GNSS.available()) {
      gps.encode(GNSS.read());
    }

    delay(200);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No se pudo conectar al AP del ESP32 del dron.");
    Serial.println("Se enviará LoRaWAN con las últimas métricas disponibles.");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    return;
  }

  Serial.println("Conectada al AP del ESP32 del dron.");
  Serial.print("IP Heltec: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI WiFi: ");
  Serial.println(WiFi.RSSI());

  udp.begin(UDP_LISTEN_PORT);

  Serial.print("Escuchando UDP en puerto ");
  Serial.println(UDP_LISTEN_PORT);

  uint32_t startListen = millis();

  while (millis() - startListen < listenMs) {
    while (GNSS.available()) {
      gps.encode(GNSS.read());
    }

    readUdpPacketIfAvailable();

    delay(10);
  }

  udp.stop();

  Serial.println("Fin ventana UDP. Apagando WiFi antes de LoRaWAN.");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(300);
}

// ----------------------------------------------------
// Lee GNSS durante una ventana de tiempo corta
// ----------------------------------------------------
static void readGpsWindow(uint32_t ms) {
  uint32_t start = millis();

  while (millis() - start < ms) {
    while (GNSS.available()) {
      gps.encode(GNSS.read());
    }
    delay(2);
  }
}

// ----------------------------------------------------
// Conversión a big-endian
// ----------------------------------------------------
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

// ----------------------------------------------------
// Prepara el payload uplink
//
// Payload de 20 bytes:
//
// byte 0      -> fix GPS: 0 no válido, 1 válido
// byte 1      -> número de satélites
// bytes 2-5   -> latitud * 1e6
// bytes 6-9   -> longitud * 1e6
// bytes 10-11 -> altitud en metros
// byte 12     -> batería dron
// bytes 13-14 -> ToF dron
// bytes 15-16 -> velocidad dron
// bytes 17-18 -> tiempo dron
// byte 19     -> SDK activo
// ----------------------------------------------------
static void prepareTxFrame(uint8_t port) {
  // Primero intentamos recibir métricas reales del ESP32 del dron.
  receiveDroneMetricsWindow(UDP_LISTEN_WINDOW_MS);

  // Después damos un pequeño margen al GNSS.
  readGpsWindow(1000);

  uint8_t fix = gps.location.isValid() ? 1 : 0;
  uint8_t sats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;

  int32_t lat = 0;
  int32_t lon = 0;
  int16_t alt = -32768;

  if (gps.location.isValid()) {
    lat = (int32_t)(gps.location.lat() * 1000000.0);
    lon = (int32_t)(gps.location.lng() * 1000000.0);
  }

  if (gps.altitude.isValid()) {
    alt = (int16_t)gps.altitude.meters();
  }

  appDataSize = 20;

  appData[0] = fix;
  appData[1] = sats;

  putInt32BE(&appData[2], lat);
  putInt32BE(&appData[6], lon);
  putInt16BE(&appData[10], alt);

  appData[12] = (int8_t)droneBattery;
  putInt16BE(&appData[13], (int16_t)droneTof);
  putInt16BE(&appData[15], (int16_t)droneSpeed);
  putInt16BE(&appData[17], (int16_t)droneTime);
  appData[19] = (uint8_t)droneSdk;

  Serial.println();
  Serial.println("----- UPLINK -----");

  Serial.print("Fix: ");
  Serial.println(fix);

  Serial.print("Satelites: ");
  Serial.println(sats);

  if (gps.location.isValid()) {
    Serial.print("Latitud: ");
    Serial.println(gps.location.lat(), 6);

    Serial.print("Longitud: ");
    Serial.println(gps.location.lng(), 6);
  } else {
    Serial.println("Posicion no valida");
  }

  if (gps.altitude.isValid()) {
    Serial.print("Altitud (m): ");
    Serial.println(gps.altitude.meters());
  } else {
    Serial.println("Altitud no valida");
  }

  Serial.println("Métricas dron recibidas/últimas disponibles:");
  Serial.print("BAT: ");
  Serial.println(droneBattery);

  Serial.print("TOF: ");
  Serial.println(droneTof);

  Serial.print("SPD: ");
  Serial.println(droneSpeed);

  Serial.print("TIME: ");
  Serial.println(droneTime);

  Serial.print("SDK: ");
  Serial.println(droneSdk);

  Serial.print("RSSI WiFi dron: ");
  Serial.println(droneWifiRssi);

  Serial.print("Payload bytes: ");
  for (int i = 0; i < appDataSize; i++) {
    if (appData[i] < 16) Serial.print("0");
    Serial.print(appData[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// ----------------------------------------------------
// SETUP
// ----------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Inicio Heltec GNSS + WiFi/UDP dron + LoRaWAN");

  Serial.print("Motivo de reinicio: ");
  Serial.println(esp_reset_reason());

  // Encender alimentación del GNSS
  pinMode(VEXT_CTRL_PIN, OUTPUT);
  digitalWrite(VEXT_CTRL_PIN, HIGH);

  // Sacar GNSS de reset
  pinMode(GNSS_RST_PIN, OUTPUT);
  digitalWrite(GNSS_RST_PIN, HIGH);
  delay(500);

  // UART del GNSS
  GNSS.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
  Serial.println("GNSS iniciado");

  // Importante: arrancamos con WiFi apagado.
  // Solo se activa durante la ventana UDP antes de cada uplink.
  WiFi.mode(WIFI_OFF);

  // Inicialización Heltec
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  Serial.println("Mcu inicializado");
}

// ----------------------------------------------------
// LOOP
// ----------------------------------------------------
void loop() {
  // Mantener alimentado el parser GNSS
  while (GNSS.available()) {
    gps.encode(GNSS.read());
  }

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
      LoRaWAN.send();

      Serial.println("Uplink enviado");
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
      while (GNSS.available()) {
        gps.encode(GNSS.read());
      }

      // Puede desconectar temporalmente el monitor serie, pero es lo que
      // permite que la librería LoRaWAN funcione correctamente.
      LoRaWAN.sleep(loraWanClass);
      break;

    default:
      deviceState = DEVICE_STATE_INIT;
      break;
  }
}