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

// -------------------- WiFi/UDP Tello Dron --------------------
const char* TELLO_SSID = "TELLO-99454F";
const char* TELLO_PASS = "";
IPAddress TELLO_IP(192, 168, 10, 1);
const uint16_t TELLO_CMD_PORT = 8889;
const uint16_t LOCAL_PORT = 8889;

WiFiUDP udp;

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
// Auxiliar: Realiza un retardo procesando bytes GNSS
// ----------------------------------------------------
void delayWithGps(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    while (GNSS.available()) {
      gps.encode(GNSS.read());
    }
    delay(2);
  }
}

// ----------------------------------------------------
// Envía un comando por UDP al SDK del dron y espera respuesta
// manteniendo alimentado el descodificador GNSS
// ----------------------------------------------------
String sendTelloCommand(const String& cmd, uint32_t timeoutMs = 1000) {
  // Limpia posibles paquetes UDP antiguos
  while (udp.parsePacket() > 0) {
    while (udp.available()) {
      udp.read();
    }
  }

  Serial.print("[TELLO TX] ");
  Serial.println(cmd);

  udp.beginPacket(TELLO_IP, TELLO_CMD_PORT);
  udp.print(cmd);
  udp.endPacket();

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (GNSS.available()) {
      gps.encode(GNSS.read());
    }

    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      char buffer[128];
      int len = udp.read(buffer, sizeof(buffer) - 1);
      if (len > 0) {
        buffer[len] = '\0';
      }

      String response = String(buffer);
      response.trim();

      Serial.print("[TELLO RX] ");
      Serial.println(response);
      return response;
    }
    delay(5);
  }

  Serial.print("[TELLO RX] TIMEOUT: ");
  Serial.println(cmd);
  return "";
}

// ----------------------------------------------------
// Convierte una respuesta de texto a un entero válido
// ----------------------------------------------------
int responseToInt(const String& response) {
  if (response.length() == 0) {
    return -1;
  }
  if (response == "ok" || response == "error") {
    return -1;
  }
  return response.toInt();
}

// ----------------------------------------------------
// Habilita el modo SDK en el dron Tello
// ----------------------------------------------------
bool enterTelloSdkMode() {
  Serial.println("Entrando en modo SDK del Tello...");
  String response = sendTelloCommand("command", 1500);
  if (response == "ok") {
    Serial.println("Modo SDK activado correctamente.");
    return true;
  }
  Serial.println("No se pudo activar el modo SDK.");
  return false;
}

// ----------------------------------------------------
// Conectar al WiFi del dron y consultar métricas UDP/SDK
// ----------------------------------------------------
void queryDroneMetrics() {
  Serial.println();
  Serial.println("----- WIFI/UDP TELLO DRON -----");
  Serial.print("Conectando al WiFi del dron: ");
  Serial.println(TELLO_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(TELLO_SSID, TELLO_PASS);

  uint32_t startConnect = millis();

  // Esperar conexión manteniendo alimentado el GNSS
  while (WiFi.status() != WL_CONNECTED && millis() - startConnect < 8000) {
    while (GNSS.available()) {
      gps.encode(GNSS.read());
    }
    delay(200);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No se pudo conectar al WiFi del dron.");
    Serial.println("Se enviará LoRaWAN con las últimas métricas disponibles.");
    droneSdk = 0;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    return;
  }

  Serial.println("Conectado al WiFi del dron.");
  Serial.print("IP Heltec: ");
  Serial.println(WiFi.localIP());
  
  droneWifiRssi = WiFi.RSSI();
  Serial.print("RSSI WiFi dron: ");
  Serial.println(droneWifiRssi);

  // Iniciar UDP local para comunicación con el dron
  if (udp.begin(LOCAL_PORT)) {
    Serial.print("UDP iniciado en puerto local ");
    Serial.println(LOCAL_PORT);

    // Activar modo SDK y consultar métricas
    bool sdkOk = enterTelloSdkMode();
    droneSdk = sdkOk ? 1 : 0;

    if (sdkOk) {
      String batteryResp = sendTelloCommand("battery?");
      droneBattery = responseToInt(batteryResp);
      delayWithGps(80);

      String tofResp = sendTelloCommand("tof?");
      droneTof = responseToInt(tofResp);
      delayWithGps(80);

      String speedResp = sendTelloCommand("speed?");
      droneSpeed = responseToInt(speedResp);
      delayWithGps(80);

      String timeResp = sendTelloCommand("time?");
      droneTime = responseToInt(timeResp);
      delayWithGps(80);
    }
    udp.stop();
  } else {
    Serial.println("Error iniciando UDP local.");
    droneSdk = 0;
  }

  Serial.println("Apagando WiFi antes de LoRaWAN.");
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
  // Primero intentamos consultar métricas reales directamente al dron.
  queryDroneMetrics();

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