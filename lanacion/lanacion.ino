// === NeoRCP ESP32 (NimBLE) ===
// Recibe "START" por BLE y, al terminar 20s, envía JSON por Notify.
// También envía por cada compresión: LIVE,<forceOk>,<rhythmCat>
//   forceOk: 0/1 (incorrecta/correcta)
//   rhythmCat: 1=lento, 2=rápido, 3=correcto

#include <NimBLEDevice.h>
#include <math.h>

// ---------------- Pines ----------------
const int botonPin  = 1;
const int sensorPin = 4;

// ---------------- Fuerza: detección por CAMBIO respecto al estado base ----------------
// Si tu sensor “activa” a LOW, dejá true. Si activa a HIGH, ponelo en false.
const bool     SENSOR_ACTIVE_LOW  = true;
const uint32_t SENSOR_DEBOUNCE_MS = 15;   // tiempo mínimo que debe durar el cambio

// Estado por compresión (solo fuerza)
int       sensorBase        = HIGH;  // nivel que tenía el sensor al empezar la compresión
uint32_t  sensorFlipSince   = 0;     // desde cuándo está distinto de base (para el debounce)

// ---------------- Lógica de entrenamiento ----------------
int estadoBotonActual   = HIGH;
int estadoBotonAnterior = HIGH;

unsigned long tiempoAnterior = 0;  // para calcular intervalo entre compresiones
unsigned long ultimoCambio   = 0;

const unsigned long intervaloMin  = 450;   // ms (≈133 cpm)
const unsigned long intervaloMax  = 650;   // ms (≈92 cpm)
const unsigned long debounceDelay = 50;    // ms
const unsigned long DURACION_MS   = 20000; // 20 s

int compresionesTotales = 0;
// "ritmoCorrecto" cuenta cuántos intervalos entre compresiones cayeron en rango.
// Nota: con N compresiones hay (N-1) intervalos posibles.
int ritmoCorrecto       = 0;
// "fuerzaCorrecta" cuenta compresiones donde hubo CAMBIO estable del sensor (respecto al estado base).
int fuerzaCorrecta      = 0;

unsigned long tiempoInicio   = 0;
bool entrenamientoIniciado   = false;
bool entrenamientoFinalizado = false;

// Banderas por compresión
bool presionado      = false;
bool fuerzaDetectada = false;  // true si se detectó cambio real (edge) del sensor

// ---------------- BLE (UUIDs) ----------------
#define SVC_UUID "f0000001-0451-4000-b000-000000000000"
#define RX_UUID  "f0000002-0451-4000-b000-000000000000" // Write (App -> ESP32)
#define TX_UUID  "f0000003-0451-4000-b000-000000000000" // Notify (ESP32 -> App)

NimBLEServer*         g_server  = nullptr;
NimBLECharacteristic* g_txChar  = nullptr;
bool g_deviceConnected = false;

// Si llega START antes de que Android termine de suscribirse, guardamos un ACK pendiente
bool g_ackStartPendiente = false;

// ---------- Utils de notificación ----------
void bleNotifyLine(const String& s) {
  if (!g_deviceConnected || g_txChar == nullptr) {
    Serial.println("No hay central conectada o TX nulo; no se notifica.");
    return;
  }
  String line = s.endsWith("\n") ? s : (s + "\n");   // SIEMPRE '\n'
  g_txChar->setValue((uint8_t*)line.c_str(), line.length());
  g_txChar->notify();
}

void sendAckStart() {
  bleNotifyLine("{\"ack\":\"start\"}");
  Serial.println("ACK START enviado");
}

// ---------------- Lógica ----------------
void resetEntrenamiento() {
  compresionesTotales = 0;
  ritmoCorrecto       = 0;
  fuerzaCorrecta      = 0;
  tiempoAnterior      = 0;
  ultimoCambio        = 0;
  presionado          = false;
  fuerzaDetectada     = false;
  sensorFlipSince     = 0;
}

void iniciarEntrenamiento() {
  entrenamientoIniciado   = true;
  entrenamientoFinalizado = false;
  resetEntrenamiento();
  tiempoInicio = millis();
  Serial.println("Entrenamiento iniciado: 20s");
}

void enviarJsonFinalYTerminar() {
  entrenamientoFinalizado = true;
  entrenamientoIniciado   = false;

  int fuerzaPct = (compresionesTotales > 0)
                    ? (int)roundf(fuerzaCorrecta * 100.0f / compresionesTotales)
                    : 0;

  int ritmoDen = (compresionesTotales > 1) ? (compresionesTotales - 1) : 0;
  int ritmoPct = (ritmoDen > 0)
                    ? (int)roundf(ritmoCorrecto * 100.0f / ritmoDen)
                    : 0;

  String json = String("{")
                + "\"fuerza\":\"" + String(fuerzaPct)           + "\","  // porcentaje 0-100
                + "\"pulsos\":\"" + String(ritmoPct)            + "\","  // porcentaje 0-100
                + "\"total\":\""  + String(compresionesTotales) + "\""
                + "}";

  Serial.println("JSON final:");
  Serial.println(json);
  bleNotifyLine(json);
}

// ---------------- Callbacks BLE ----------------
class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    g_deviceConnected = true;
    Serial.printf("Central conectada: %s\n", info.getAddress().toString().c_str());
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    g_deviceConnected = false;
    Serial.println("Central desconectada. Re-Advertising…");
    NimBLEDevice::startAdvertising();
  }
};

class TxCB : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic* c, NimBLEConnInfo& info, uint16_t subValue) override {
    Serial.printf("TX subscribed from %s, value=0x%04x\n",
                  info.getAddress().toString().c_str(), subValue);
    if (g_ackStartPendiente) {
      sendAckStart();
      g_ackStartPendiente = false;
    }
  }
};

class RxCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    std::string v = c->getValue();
    if (v.empty()) return;
    String cmd; for (char ch: v) cmd += ch;
    cmd.trim(); cmd.toUpperCase();
    Serial.print("CMD: "); Serial.println(cmd);

    if (cmd == "START") {
      if (g_deviceConnected) sendAckStart();
      else g_ackStartPendiente = true;

      iniciarEntrenamiento();
    } else if (cmd == "STOP") {
      entrenamientoIniciado = false;
      entrenamientoFinalizado = true;
      Serial.println("STOP recibido");
    }
  }
};

// ---------------- Setup/Loop ----------------
void setupBLE() {
  NimBLEDevice::init("NeoRCP");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(185);

  g_server = NimBLEDevice::createServer();
  g_server->setCallbacks(new ServerCB());

  auto svc = g_server->createService(SVC_UUID);

  // TX: Notify (resultados)
  g_txChar = svc->createCharacteristic(
      TX_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  g_txChar->setCallbacks(new TxCB());

  // RX: Write (comandos)
  auto rx = svc->createCharacteristic(
      RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  rx->setCallbacks(new RxCB());

  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName("NeoRCP");
  adv->addServiceUUID(SVC_UUID);
  adv->enableScanResponse(true);
  adv->start();

  Serial.println("Advertising iniciado: NeoRCP");
}

void setup() {
  Serial.begin(115200);
  pinMode(botonPin, INPUT_PULLUP);
  pinMode(sensorPin, INPUT);        // Si tu sensor es colector abierto, probá INPUT_PULLUP
  setupBLE();
  Serial.println("Esperando START por BLE (o 'YA' por Serial).");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n'); cmd.trim(); cmd.toUpperCase();
    if (cmd == "YA") iniciarEntrenamiento();
  }

  if (!entrenamientoIniciado || entrenamientoFinalizado) return;

  // Timeout
  if (millis() - tiempoInicio >= DURACION_MS) {
    enviarJsonFinalYTerminar();
    return;
  }

  unsigned long t = millis();
  estadoBotonActual = digitalRead(botonPin);
  int estadoSensor  = digitalRead(sensorPin);

  // Flanco de bajada (inicio de compresión)
  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW && (t - ultimoCambio) > debounceDelay) {
    ultimoCambio    = t;
    presionado      = true;

    // --- Fuerza: reset de estado por compresión + baseline del sensor ---
    fuerzaDetectada = false;
    sensorFlipSince = 0;
    sensorBase      = digitalRead(sensorPin);  // guardamos el estado “en reposo” para esta compresión
  }

  if (presionado) {
    // --- Fuerza: exige CAMBIO real del sensor (edge) estable respecto a 'sensorBase' ---
    uint32_t now = t;
    int s = estadoSensor;

    bool isActive    = SENSOR_ACTIVE_LOW ? (s == LOW) : (s == HIGH);
    bool baseActive  = SENSOR_ACTIVE_LOW ? (sensorBase == LOW) : (sensorBase == HIGH);
    bool changed     = (isActive != baseActive);   // distinto al estado con el que empezó la compresión

    if (changed) {
      if (sensorFlipSince == 0) sensorFlipSince = now;
      if (!fuerzaDetectada && (now - sensorFlipSince >= SENSOR_DEBOUNCE_MS)) {
        fuerzaDetectada = true;  // hubo cambio estable ⇒ fuerza correcta
      }
    } else {
      sensorFlipSince = 0;       // volvió al estado base; reinicio temporizador de cambio
    }

    // Flanco de subida = cierre de compresión
    if (estadoBotonAnterior == LOW && estadoBotonActual == HIGH && (t - ultimoCambio) > debounceDelay) {
      ultimoCambio = t;
      presionado   = false;
      compresionesTotales++;

      // Fuerza correcta si hubo cambio estable del sensor alguna vez durante la compresión
      if (fuerzaDetectada) fuerzaCorrecta++;

      unsigned long intervalo = t - tiempoAnterior;
      if (tiempoAnterior > 0) {
        if (intervalo >= intervaloMin && intervalo <= intervaloMax) ritmoCorrecto++;
      }

      // === LIVE por compresión ===
      uint8_t forceOk = fuerzaDetectada ? 1 : 0;

      // 1=lento, 2=rápido, 3=correcto (para la primera compresión no hay ritmo)
      uint8_t rhythmCat = 0;
      if (tiempoAnterior > 0) {
        if (intervalo < intervaloMin)       rhythmCat = 2; // muy rápido
        else if (intervalo > intervaloMax)  rhythmCat = 1; // muy lento
        else                                rhythmCat = 3; // correcto
      }

      if (rhythmCat != 0) {
        String live = String("LIVE,") + String(forceOk) + "," + String(rhythmCat);
        bleNotifyLine(live); // '\n' incluido
      }

      tiempoAnterior = t;
    }
  }

  estadoBotonAnterior = estadoBotonActual;
}
