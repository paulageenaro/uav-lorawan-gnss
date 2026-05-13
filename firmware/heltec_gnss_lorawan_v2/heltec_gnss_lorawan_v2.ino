#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "HT_TinyGPS++.h"
#include "esp_system.h"

HardwareSerial GNSS(1);
TinyGPSPlus gps;

// -------------------- Pines GNSS --------------------
static const int VEXT_CTRL_PIN = 3;    // Alimentación del GNSS
static const int GNSS_RX_PIN   = 33;   // ESP32 RX <- GNSS TX
static const int GNSS_TX_PIN   = 34;   // ESP32 TX -> GNSS RX
static const int GNSS_RST_PIN  = 35;   // Reset del GNSS

// -------------------- OTAA keys --------------------
// Estos valores deben coincidir exactamente con ChirpStack
uint8_t devEui[] = {
  0xA4, 0xCF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0x01
};

uint8_t appEui[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
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

uint32_t appTxDutyCycle = 30000;   // 30 segundos entre envíos
bool overTheAirActivation = true;  // OTAA
bool loraWanAdr = true;            // ADR activado
bool isTxConfirmed = false;        // Uplink no confirmado
uint8_t appPort = 2;               // Puerto LoRaWAN
uint8_t confirmedNbTrials = 4;

// ----------------------------------------------------
// Lee datos GNSS durante una ventana de tiempo
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
// Prepara el payload LoRaWAN
//
// Formato del payload:
// byte 0      -> fix GPS: 0 no válido, 1 válido
// byte 1      -> número de satélites
// bytes 2-5   -> latitud * 1e6
// bytes 6-9   -> longitud * 1e6
// bytes 10-11 -> altitud en metros
// ----------------------------------------------------
static void prepareTxFrame(uint8_t port) {
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

  appDataSize = 12;

  appData[0] = fix;
  appData[1] = sats;
  putInt32BE(&appData[2], lat);
  putInt32BE(&appData[6], lon);
  putInt16BE(&appData[10], alt);

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
  delay(2000);

  Serial.println();
  Serial.println("Inicio de mota GNSS + LoRaWAN");

  Serial.print("Motivo de reinicio: ");
  Serial.println(esp_reset_reason());

  // Encender alimentación del GNSS
  pinMode(VEXT_CTRL_PIN, OUTPUT);
  digitalWrite(VEXT_CTRL_PIN, HIGH);

  // Sacar GNSS de reset
  pinMode(GNSS_RST_PIN, OUTPUT);
  digitalWrite(GNSS_RST_PIN, HIGH);
  delay(500);

  // Inicializar UART del GNSS
  GNSS.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
  Serial.println("GNSS iniciado");

  // Inicialización de la placa Heltec
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  Serial.println("Mcu inicializado");
}

// ----------------------------------------------------
// LOOP
// ----------------------------------------------------
void loop() {
  // Mantener alimentado el parser GNSS siempre que haya datos
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
      txDutyCycleTime = appTxDutyCycle;

      Serial.print("Esperando ");
      Serial.print(txDutyCycleTime / 1000);
      Serial.println(" segundos sin deep sleep...");

      delay(txDutyCycleTime);

      deviceState = DEVICE_STATE_SEND;
      break;

    case DEVICE_STATE_SLEEP:
      // Importante:
      // No usamos LoRaWAN.sleep() durante la depuracion,
      // porque puede hacer que el USB se desconecte y vuelva a conectarse.

      while (GNSS.available()) {
        gps.encode(GNSS.read());
      }

      delay(100);
      deviceState = DEVICE_STATE_SEND;
      break;

    default:
      deviceState = DEVICE_STATE_INIT;
      break;
  }
}
