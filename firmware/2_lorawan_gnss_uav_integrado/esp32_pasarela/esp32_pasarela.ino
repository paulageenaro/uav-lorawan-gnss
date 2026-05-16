#include <WiFi.h>
#include <WiFiUdp.h>

// ======================================================
// CONFIGURACIÓN WIFI DEL DRON TELLO / ROBOMASTER TT
// ======================================================

// Cambia esto por el nombre WiFi real del dron
const char* TELLO_SSID = "TELLO-99454F";

// Normalmente el Tello no tiene contraseña
const char* TELLO_PASS = "";

// IP y puerto estándar del Tello SDK
IPAddress TELLO_IP(192, 168, 10, 1);
const uint16_t TELLO_CMD_PORT = 8889;

// Puerto local desde el que el ESP32 enviará comandos al Tello
const uint16_t LOCAL_TELLO_PORT = 8889;


// ======================================================
// CONFIGURACIÓN WIFI PARA LA HELTEC
// ======================================================

// Red WiFi que crea el ESP32 para que se conecte la Heltec
const char* AP_SSID = "UAV_METRICS_AP";
const char* AP_PASS = "12345678";   // mínimo 8 caracteres

// IP del ESP32 en su red propia
IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

// Enviamos por broadcast para no depender de la IP concreta de la Heltec
IPAddress HELTEC_BROADCAST_IP(192, 168, 4, 255);

// Puerto UDP donde escuchará la Heltec
const uint16_t HELTEC_UDP_PORT = 4210;


// ======================================================
// OBJETOS UDP
// ======================================================

WiFiUDP telloUdp;
WiFiUDP heltecUdp;


// ======================================================
// VARIABLES DE MÉTRICAS
// ======================================================

int batteryValue = -1;
int tofValue = -1;
int speedValue = -1;
int timeValue = -1;

bool telloSdkOk = false;

unsigned long lastMetricsTime = 0;
const unsigned long METRICS_PERIOD_MS = 2000;


// ======================================================
// FUNCIONES AUXILIARES
// ======================================================

String sendTelloCommand(const String& cmd, uint32_t timeoutMs = 1500) {
  // Limpia posibles paquetes UDP antiguos
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

  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    int packetSize = telloUdp.parsePacket();

    if (packetSize > 0) {
      char buffer[128];
      int len = telloUdp.read(buffer, sizeof(buffer) - 1);
      if (len > 0) {
        buffer[len] = '\0';
      }

      String response = String(buffer);
      response.trim();

      Serial.print("[TELLO RX] ");
      Serial.println(response);

      return response;
    }

    delay(10);
  }

  Serial.print("[TELLO RX] TIMEOUT en comando: ");
  Serial.println(cmd);

  return "";
}


int responseToInt(const String& response) {
  if (response.length() == 0) {
    return -1;
  }

  // Algunos comandos devuelven solo un número.
  // Si devuelve "ok" o "error", no es métrica válida.
  if (response == "ok" || response == "error") {
    return -1;
  }

  return response.toInt();
}


bool enterTelloSdkMode() {
  Serial.println();
  Serial.println("Entrando en modo SDK del Tello...");

  String response = sendTelloCommand("command", 2000);

  if (response == "ok") {
    Serial.println("Modo SDK activado correctamente.");
    return true;
  }

  Serial.println("No se ha podido activar el modo SDK.");
  return false;
}


void readTelloMetrics() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ESP32 no conectado al WiFi del dron. No se leen métricas.");
    telloSdkOk = false;
    return;
  }

  if (!telloSdkOk) {
    telloSdkOk = enterTelloSdkMode();
    if (!telloSdkOk) {
      return;
    }
  }

  String batteryResp = sendTelloCommand("battery?");
  batteryValue = responseToInt(batteryResp);

  delay(80);

  String tofResp = sendTelloCommand("tof?");
  tofValue = responseToInt(tofResp);

  delay(80);

  String speedResp = sendTelloCommand("speed?");
  speedValue = responseToInt(speedResp);

  delay(80);

  String timeResp = sendTelloCommand("time?");
  timeValue = responseToInt(timeResp);

  delay(80);
}


String buildMetricsMessage() {
  String msg = "";

  msg += "BAT=";
  msg += batteryValue;

  msg += ";TOF=";
  msg += tofValue;

  msg += ";SPD=";
  msg += speedValue;

  msg += ";TIME=";
  msg += timeValue;

  msg += ";SDK=";
  msg += telloSdkOk ? 1 : 0;

  msg += ";RSSI=";
  msg += WiFi.RSSI();

  return msg;
}


void sendMetricsToHeltec(const String& msg) {
  Serial.print("[HELTEC UDP TX] ");
  Serial.println(msg);

  heltecUdp.beginPacket(HELTEC_BROADCAST_IP, HELTEC_UDP_PORT);
  heltecUdp.print(msg);
  heltecUdp.endPacket();
}


void connectToTelloWiFi() {
  Serial.println();
  Serial.print("Conectando al WiFi del dron: ");
  Serial.println(TELLO_SSID);

  WiFi.begin(TELLO_SSID, TELLO_PASS);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conectado al WiFi del dron.");
    Serial.print("IP STA del ESP32: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI WiFi dron: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("No se ha podido conectar al WiFi del dron.");
    Serial.println("El AP para la Heltec seguirá activo.");
  }
}


void setupHeltecAccessPoint() {
  Serial.println();
  Serial.println("Creando red WiFi para la Heltec...");

  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  bool ok = WiFi.softAP(AP_SSID, AP_PASS);

  if (ok) {
    Serial.println("AP creado correctamente.");
    Serial.print("SSID AP: ");
    Serial.println(AP_SSID);
    Serial.print("IP AP ESP32: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Error creando el AP.");
  }
}


// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("=======================================");
  Serial.println("ESP32 KIT UAV - Tello metrics UDP bridge");
  Serial.println("=======================================");

  // Modo doble:
  // STA: conexión al WiFi del dron
  // AP: red para la Heltec
  WiFi.mode(WIFI_AP_STA);
  delay(500);

  setupHeltecAccessPoint();
  connectToTelloWiFi();

  // UDP para hablar con el Tello
  if (telloUdp.begin(LOCAL_TELLO_PORT)) {
    Serial.print("UDP Tello iniciado en puerto local ");
    Serial.println(LOCAL_TELLO_PORT);
  } else {
    Serial.println("Error iniciando UDP Tello.");
  }

  // UDP para enviar a la Heltec
  heltecUdp.begin(HELTEC_UDP_PORT + 1);

  // Intentar entrar en modo SDK al arrancar
  if (WiFi.status() == WL_CONNECTED) {
    telloSdkOk = enterTelloSdkMode();
  }

  Serial.println("Sistema iniciado.");
}


// ======================================================
// LOOP
// ======================================================

void loop() {
  // Si se pierde la conexión con el dron, intenta reconectar
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;

    if (millis() - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = millis();

      Serial.println();
      Serial.println("WiFi del dron desconectado. Intentando reconectar...");
      telloSdkOk = false;
      connectToTelloWiFi();
    }
  }

  // Leer y enviar métricas periódicamente
  if (millis() - lastMetricsTime >= METRICS_PERIOD_MS) {
    lastMetricsTime = millis();

    readTelloMetrics();

    String msg = buildMetricsMessage();
    sendMetricsToHeltec(msg);
  }
}