#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "HT_TinyGPS++.h"

HardwareSerial GNSS(1);
TinyGPSPlus gps;

// -------------------- Pines GNSS --------------------
static const int VEXT_CTRL_PIN = 3;    // para dar alimentación al GNSS
static const int GNSS_RX_PIN   = 33;   // ESP32 RX <- GNSS TX, pin por donde la placa recibe datos del GNSS
static const int GNSS_TX_PIN   = 34;   // ESP32 TX -> GNSS RX, pin por donde la placa envía datos al GNSS
static const int GNSS_RST_PIN  = 35;   // pin para sacar el GNSS del reset

// -------------------- OTAA keys --------------------
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
uint16_t userChannelsMask[6] = { // qué canales LoRaWAN puede usar
  0x00FF, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000
};

LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;   // la región LoRaWAN configurada en la librería (REGION_EU868)
DeviceClass_t loraWanClass = CLASS_A;            // modo más básico y típico de LoRaWAN

uint32_t appTxDutyCycle = 30000;   // 30 s entre envíos
bool overTheAirActivation = true;  // OTAA
bool loraWanAdr = true;            // ADR activado, el servidor puede ajustar parámetros de radio
bool isTxConfirmed = false;        // los mensajes son no confirmados
uint8_t appPort = 2;               // usa el puerto 2 en LoRaWAN
uint8_t confirmedNbTrials = 4;     // si fueran confirmados, reintentaría hasta 4 veces

// ----------------------------------------------------
// Lee GNSS durante una ventana de tiempo corta
// ----------------------------------------------------
static void readGpsWindow(uint32_t ms) {
  uint32_t start = millis();       // Guarda el momento en el que empieza
  while (millis() - start < ms) {  // Repite mientras no haya pasado el tiempo indicado
    while (GNSS.available()) {     // Mientras haya datos del GNSS:
      gps.encode(GNSS.read());     // se lo pasa al objeto gps y gps va reconstruyendo la información de posición
    }
    delay(2);                      // Pausa muy pequeña para no ir demasiado rápido.
  }
}

// ----------------------------------------------------
// Conversión a big-endian para el payload (BE = Big Endian)
// ----------------------------------------------------
static void putInt32BE(uint8_t *buf, int32_t v) {  // convierte un entero de 32 bits → en 4 bytes
  buf[0] = (uint8_t)((v >> 24) & 0xFF);
  buf[1] = (uint8_t)((v >> 16) & 0xFF);
  buf[2] = (uint8_t)((v >> 8) & 0xFF);
  buf[3] = (uint8_t)(v & 0xFF);
}

static void putInt16BE(uint8_t *buf, int16_t v) {   // convierte un entero de 16 bits → en 2 bytes
  buf[0] = (uint8_t)((v >> 8) & 0xFF);
  buf[1] = (uint8_t)(v & 0xFF);
}

// ----------------------------------------------------
// Prepara el payload uplink (12 bytes)
//
// Formato:
// byte 0     -> fix (0/1)
// byte 1     -> satélites
// bytes 2-5  -> latitud * 1e6 (int32)
// bytes 6-9  -> longitud * 1e6 (int32)
// bytes 10-11-> altitud en metros (int16)
// ----------------------------------------------------
static void prepareTxFrame(uint8_t port) {
  readGpsWindow(1000); // lee el GPS durante 1 segundo.

  uint8_t fix = gps.location.isValid() ? 1 : 0; // fix → vale 1 si hay posición válida, 0 si no
  uint8_t sats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0; // sats → número de satélites, o 0 si no se sabe

  int32_t lat = 0;
  int32_t lon = 0;
  int16_t alt = -32768;   // valor especial "sin dato", -32768 = “no tengo altitud válida”

  if (gps.location.isValid()) { // Si la localización es válida, guarda latitud y longitud
    // Para no mandar decimales:
    lat = (int32_t)(gps.location.lat() * 1000000.0);
    lon = (int32_t)(gps.location.lng() * 1000000.0);
  }

  if (gps.altitude.isValid()) { // Si la altitud es válida, la guarda (en metros)
    alt = (int16_t)gps.altitude.meters();
  }

// --------------- Construcción del Payload ---------------

  appDataSize = 12;
  appData[0] = fix;
  appData[1] = sats;
  putInt32BE(&appData[2], lat);
  putInt32BE(&appData[6], lon);
  putInt16BE(&appData[10], alt);

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
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Inicio de mota GNSS + LoRaWAN");

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

  // Inicialización Heltec
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
}

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
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;

    case DEVICE_STATE_SLEEP:
      while (GNSS.available()) {
        gps.encode(GNSS.read());
      }
      LoRaWAN.sleep(loraWanClass);
      break;

    default:
      deviceState = DEVICE_STATE_INIT;
      break;
  }
}
