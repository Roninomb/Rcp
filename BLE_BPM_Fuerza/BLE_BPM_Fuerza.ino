#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Pines
const int botonPin = 35;
const int sensorPin = 34;

// Estado del botón
int estadoBotonActual = HIGH;
int estadoBotonAnterior = HIGH;

// Temporizadores
unsigned long tiempoAnterior = 0;
unsigned long ultimoCambio = 0;
unsigned long tiempoInicio = 0;

// Intervalos permitidos
const unsigned long intervaloMin = 450;
const unsigned long intervaloMax = 650;
const unsigned long debounceDelay = 50;

// Métricas de entrenamiento
float PrimeraCompresion = 1;
int compresionesTotales = 0;
int ritmoCorrecto = 0;
int fuerzaCorrecta = 0;

// Estados del entrenamiento
bool entrenamientoIniciado = false;
bool entrenamientoFinalizado = false;

// Flags internos
bool presionado = false;
bool fuerzaDetectada = false;
bool fuerzaLiberada = false;

// BLE
BLECharacteristic* pCharacteristic;
bool bleConectado = false;

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConectado = true;
    Serial.println("📲 App conectada");
  }

  void onDisconnect(BLEServer* pServer) {
    bleConectado = false;
    Serial.println("📴 App desconectada");
  }
};

void iniciarBLE() {
  BLEDevice::init("NeoRCP");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("🟢 BLE iniciado");
}

void setup() {
  pinMode(botonPin, INPUT_PULLUP);
  pinMode(sensorPin, INPUT);
  Serial.begin(115200);

  Serial.println("📢 Escribí 'ya' en el Monitor Serie para comenzar el entrenamiento de 30 segundos.");

  iniciarBLE();
}

void loop() {
  // Inicio del entrenamiento
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando.equalsIgnoreCase("ya")) {
      if (!entrenamientoIniciado || entrenamientoFinalizado) {
        entrenamientoIniciado = true;
        entrenamientoFinalizado = false;
        compresionesTotales = 0;
        ritmoCorrecto = 0;
        fuerzaCorrecta = 0;
        tiempoAnterior = 0;
        ultimoCambio = 0;
        tiempoInicio = millis();
        presionado = false;
        fuerzaDetectada = false;
        fuerzaLiberada = false;
        Serial.println("🔁 Nuevo entrenamiento iniciado: tenés 20 segundos para hacer compresiones.");
      }
    }
  }

  if (!entrenamientoIniciado || entrenamientoFinalizado) return;

  // Final del entrenamiento
  if (millis() - tiempoInicio >= 20000) {
    entrenamientoFinalizado = true;

    Serial.println("⏹️ Tiempo finalizado.");
    float compresionesTotales2 = compresionesTotales - PrimeraCompresion;
    float porcentajeRitmo = (compresionesTotales2 > 0) ? (ritmoCorrecto * 100.0 / compresionesTotales2) : 0;
    float porcentajeFuerza = (compresionesTotales > 0) ? (fuerzaCorrecta * 100.0 / compresionesTotales) : 0;
    float porcentajeGeneral = (porcentajeRitmo + porcentajeFuerza) / 2.0;

    Serial.print("🧮 Total de compresiones: ");
    Serial.println(compresionesTotales);

    Serial.print("🎯 Ritmo correcto: ");
    Serial.print(ritmoCorrecto);
    Serial.print(" (");
    Serial.print(porcentajeRitmo, 1);
    Serial.println("%)");

    Serial.print("💪 Fuerza correcta: ");
    Serial.print(fuerzaCorrecta);
    Serial.print(" (");
    Serial.print(porcentajeFuerza, 1);
    Serial.println("%)");

    Serial.print("📊 Evaluación general: ");
    Serial.print(porcentajeGeneral, 1);
    Serial.println("% de efectividad");

    // 🔄 Enviar por BLE
    String resumen = "comp=" + String(compresionesTotales) +
                     ",ritmo=" + String(ritmoCorrecto) +
                     ",ritmo_pct=" + String(porcentajeRitmo, 1) +
                     ",fuerza=" + String(fuerzaCorrecta) +
                     ",fuerza_pct=" + String(porcentajeFuerza, 1) +
                     ",eval=" + String(porcentajeGeneral, 1);

    Serial.print("📤 Enviando datos a la app: ");
    Serial.println(resumen);

    if (bleConectado) {
      pCharacteristic->setValue(resumen.c_str());
      pCharacteristic->notify();
    } else {
      Serial.println("❌ BLE no conectado. No se enviaron los datos.");
    }

    Serial.println("✅ Entrenamiento terminado. Escribí 'ya' para reiniciar.");
    return;
  }

  // Lectura del botón y sensor
  unsigned long tiempoAhora = millis();
  estadoBotonActual = digitalRead(botonPin);
  int estadoSensor = digitalRead(sensorPin);

  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW && (tiempoAhora - ultimoCambio) > debounceDelay) {
    ultimoCambio = tiempoAhora;
    presionado = true;
    fuerzaDetectada = false;
    fuerzaLiberada = false;
  }

  if (presionado) {
    if (!fuerzaDetectada && estadoSensor == LOW) {
      fuerzaDetectada = true;
    }

    if (fuerzaDetectada && !fuerzaLiberada && estadoSensor == HIGH) {
      fuerzaLiberada = true;
    }

    if (estadoBotonAnterior == LOW && estadoBotonActual == HIGH && (tiempoAhora - ultimoCambio) > debounceDelay) {
      ultimoCambio = tiempoAhora;
      presionado = false;
      compresionesTotales++;

      if (fuerzaDetectada && fuerzaLiberada) {
        fuerzaCorrecta++;
        Serial.println("✅ Fuerza correcta");
      } else {
        Serial.println("⚠️ Fuerza insuficiente");
      }

      unsigned long intervalo = tiempoAhora - tiempoAnterior;
      if (intervalo >= intervaloMin && intervalo <= intervaloMax) {
        ritmoCorrecto++;
        Serial.print("✅ Ritmo correcto: ");
      } else {
        Serial.print("⚠️ Ritmo incorrecto: ");
      }
      Serial.print(intervalo);
      Serial.println(" ms");

      tiempoAnterior = tiempoAhora;
    }
  }

  estadoBotonAnterior = estadoBotonActual;
}
