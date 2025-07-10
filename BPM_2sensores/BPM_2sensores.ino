const int botonPin = 33;
const int sensorPin1 = 34; // Sensor de fuerza débil
const int sensorPin2 = 35; // Sensor de fuerza fuerte

int estadoBotonActual = HIGH;
int estadoBotonAnterior = HIGH;
float PrimeraCompresion = 1;
unsigned long tiempoAnterior = 0;
unsigned long ultimoCambio = 0;

const unsigned long intervaloMin = 450;
const unsigned long intervaloMax = 650;
const unsigned long debounceDelay = 50;

int compresionesTotales = 0;
int ritmoCorrecto = 0;
int fuerzaCorrecta = 0;

unsigned long tiempoInicio = 0;
bool entrenamientoIniciado = false;
bool entrenamientoFinalizado = false;

bool presionado = false;
bool sensor1Detectado = false;
bool sensor2Detectado = false;

void setup() {
  pinMode(botonPin, INPUT_PULLUP);
  pinMode(sensorPin1, INPUT);
  pinMode(sensorPin2, INPUT);
  Serial.begin(115200);
  Serial.println("📢 Escribí 'ya' en el Monitor Serie para comenzar el entrenamiento de 20 segundos.");
}

void loop() {
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
        sensor1Detectado = false;
        sensor2Detectado = false;
        Serial.println("🔁 Nuevo entrenamiento iniciado: tenés 20 segundos para hacer compresiones.");
      }
    }
  }

  if (!entrenamientoIniciado || entrenamientoFinalizado) return;

  if (millis() - tiempoInicio >= 20000) {
    entrenamientoFinalizado = true;

    Serial.println("⏹️ Tiempo finalizado.");
    float compresionesTotales2 = compresionesTotales - PrimeraCompresion;
    float porcentajeRitmo = (compresionesTotales > 0) ? (ritmoCorrecto * 100.0 / compresionesTotales2) : 0;
    float porcentajeFuerza = (compresionesTotales > 0) ? (fuerzaCorrecta * 100.0 / compresionesTotales) : 0;
    float porcentajeGeneral = (porcentajeRitmo + porcentajeFuerza) / 2.0;

    Serial.print("🧮 Total de compresiones: "); Serial.println(compresionesTotales);
    Serial.print("🎯 Ritmo correcto: "); Serial.print(ritmoCorrecto); Serial.print(" ("); Serial.print(porcentajeRitmo, 1); Serial.println("%)");
    Serial.print("💪 Fuerza correcta: "); Serial.print(fuerzaCorrecta); Serial.print(" ("); Serial.print(porcentajeFuerza, 1); Serial.println("%)");
    Serial.print("📊 Evaluación general: "); Serial.print(porcentajeGeneral, 1); Serial.println("% de efectividad");
    Serial.println("✅ Entrenamiento terminado. Escribí 'ya' para reiniciar.");
    return;
  }

  unsigned long tiempoAhora = millis();
  estadoBotonActual = digitalRead(botonPin);
  int lectura1 = digitalRead(sensorPin1);
  int lectura2 = digitalRead(sensorPin2);

  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW && (tiempoAhora - ultimoCambio) > debounceDelay) {
    ultimoCambio = tiempoAhora;
    presionado = true;
    sensor1Detectado = false;
    sensor2Detectado = false;
  }

  if (presionado) {
    if (!sensor1Detectado && lectura1 == LOW) sensor1Detectado = true;
    if (!sensor2Detectado && lectura2 == LOW) sensor2Detectado = true;

    if (estadoBotonAnterior == LOW && estadoBotonActual == HIGH && (tiempoAhora - ultimoCambio) > debounceDelay) {
      ultimoCambio = tiempoAhora;
      presionado = false;
      compresionesTotales++;

      // --- Evaluación de fuerza con 2 sensores ---
      if (sensor1Detectado && sensor2Detectado) {
        fuerzaCorrecta++;
        Serial.println("✅ Fuerza correcta");
      } else if (sensor1Detectado && !sensor2Detectado) {
        Serial.println("⚠️ Fuerza insuficiente");
      } else if (!sensor1Detectado && sensor2Detectado) {
        Serial.println("⚠️ Fuerza excesiva");
      } else {
        Serial.println("⚠️ Sin fuerza detectada");
      }

      // --- Evaluación de ritmo ---
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
