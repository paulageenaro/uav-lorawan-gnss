#include "Arduino.h"
#include "HT_TinyGPS++.h"

// -------------------- GNSS --------------------
HardwareSerial GNSS(1);
TinyGPSPlus gps;

// -------------------- Pines GNSS --------------------
static const int VEXT_CTRL_PIN = 3;    // Alimentación del GNSS
static const int GNSS_RX_PIN   = 33;   // ESP32 RX <- GNSS TX
static const int GNSS_TX_PIN   = 34;   // ESP32 TX -> GNSS RX
static const int GNSS_RST_PIN  = 35;   // Reset del GNSS

// Cada cuánto imprimir información
unsigned long lastPrint = 0;
const unsigned long printInterval = 2000; // 2 segundos

// ----------------------------------------------------
// Lee datos del GNSS y se los pasa al parser TinyGPS++
// ----------------------------------------------------
void readGNSS() {
  while (GNSS.available()) {
    char c = GNSS.read();
    gps.encode(c);

    // Descomentar para ver las tramas NMEA en bruto
    // Serial.write(c);
  }
}

// ----------------------------------------------------
// Imprime la información GNSS por el monitor serie
// ----------------------------------------------------
void printGNSSInfo() {
  Serial.println();
  Serial.println("========== ESTADO GNSS ==========");

  Serial.print("Caracteres recibidos del GNSS: ");
  Serial.println(gps.charsProcessed());

  Serial.print("Sentencias validas: ");
  Serial.println(gps.sentencesWithFix());

  Serial.print("Satellites valid?: ");
  Serial.println(gps.satellites.isValid() ? "SI" : "NO");

  Serial.print("Numero de satelites: ");
  if (gps.satellites.isValid()) {
    Serial.println(gps.satellites.value());
  } else {
    Serial.println("sin dato");
  }

  Serial.print("Posicion valida?: ");
  Serial.println(gps.location.isValid() ? "SI" : "NO");

  if (gps.location.isValid()) {
    Serial.print("Latitud: ");
    Serial.println(gps.location.lat(), 6);

    Serial.print("Longitud: ");
    Serial.println(gps.location.lng(), 6);

    Serial.print("Edad de la posicion (ms): ");
    Serial.println(gps.location.age());
  } else {
    Serial.println("Latitud: sin fix");
    Serial.println("Longitud: sin fix");
  }

  Serial.print("Altitud valida?: ");
  Serial.println(gps.altitude.isValid() ? "SI" : "NO");

  if (gps.altitude.isValid()) {
    Serial.print("Altitud (m): ");
    Serial.println(gps.altitude.meters());
  } else {
    Serial.println("Altitud: sin dato");
  }

  Serial.print("Fecha valida?: ");
  Serial.println(gps.date.isValid() ? "SI" : "NO");

  if (gps.date.isValid()) {
    Serial.print("Fecha: ");
    Serial.print(gps.date.day());
    Serial.print("/");
    Serial.print(gps.date.month());
    Serial.print("/");
    Serial.println(gps.date.year());
  }

  Serial.print("Hora valida?: ");
  Serial.println(gps.time.isValid() ? "SI" : "NO");

  if (gps.time.isValid()) {
    Serial.print("Hora UTC: ");
    Serial.print(gps.time.hour());
    Serial.print(":");
    Serial.print(gps.time.minute());
    Serial.print(":");
    Serial.println(gps.time.second());
  }

  Serial.println("=================================");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Inicio prueba GNSS sin LoRaWAN");

  // Alimentar GNSS
  pinMode(VEXT_CTRL_PIN, OUTPUT);
  digitalWrite(VEXT_CTRL_PIN, HIGH);

  // Sacar GNSS de reset
  pinMode(GNSS_RST_PIN, OUTPUT);
  digitalWrite(GNSS_RST_PIN, HIGH);
  delay(500);

  // Iniciar UART del GNSS
  GNSS.begin(115200, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);

  Serial.println("GNSS iniciado");
  Serial.println("Coloca la placa en exterior o cerca de cielo abierto.");
  Serial.println("Puede tardar varios minutos en conseguir fix.");
}

void loop() {
  // Leer continuamente datos del GNSS
  readGNSS();

  // Imprimir cada 2 segundos
  if (millis() - lastPrint >= printInterval) {
    lastPrint = millis();
    printGNSSInfo();
  }
}
