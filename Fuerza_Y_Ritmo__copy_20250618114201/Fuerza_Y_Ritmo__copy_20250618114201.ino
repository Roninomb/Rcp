const int botonPin = 15;
const int sensor1Pin = 16;
const int sensor2Pin = 17;

int estadoBotonActual = HIGH;
int estadoBotonAnterior = HIGH;

unsigned long tiempoAnterior = 0;
unsigned long ultimoCambio = 0;

const unsigned long intervaloMin = 400;
const unsigned long intervaloMax = 700;
const unsigned long debounceDelay = 50;

int compresionesTotales = 0;
int ritmoCorrecto = 0;
int fuerzaCorrecta = 0;

unsigned long tiempoInicio = 0;
bool entrenamientoIniciado = false;
bool entrenamientoFinalizado = false;

void setup() {
  pinMode(botonPin, INPUT_PULLUP);
  pinMode(sensor1Pin, INPUT);
  pinMode(sensor2Pin, INPUT);
  Serial.begin(115200);
  Serial.println("📢 Escribí 'ya' en el Monitor Serie para comenzar el entrenamiento de 30 segundos.");
}

void loop() {
  // Verificar si hay comandos por el monitor serie
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando.equalsIgnoreCase("ya")) {
      if (!entrenamientoIniciado || entrenamientoFinalizado) {
        // Reiniciar variables
        entrenamientoIniciado = true;
        entrenamientoFinalizado = false;
        compresionesTotales = 0;
        ritmoCorrecto = 0;
        fuerzaCorrecta = 0;
        tiempoAnterior = 0;
        ultimoCambio = 0;
        tiempoInicio = millis();

        Serial.println("🔁 Nuevo entrenamiento iniciado: tenés 30 segundos para hacer compresiones.");
      }
    }
  }

  // No hacer nada si el entrenamiento no está en curso
  if (!entrenamientoIniciado || entrenamientoFinalizado) return;

  // Chequear si pasaron los 30 segundos
  if (millis() - tiempoInicio >= 30000) {
    entrenamientoFinalizado = true;

    Serial.println("⏹️ Tiempo finalizado.");

    // Calcular resultados
    float porcentajeRitmo = (compresionesTotales > 0) ? (ritmoCorrecto * 100.0 / compresionesTotales) : 0;
    float porcentajeFuerza = (compresionesTotales > 0) ? (fuerzaCorrecta * 100.0 / compresionesTotales) : 0;
    float porcentajeGeneral = (porcentajeRitmo + porcentajeFuerza) / 2.0;

    // Mostrar devoluciones
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

    Serial.println("✅ Entrenamiento terminado. Escribí 'ya' para reiniciar.");
    return;
  }

  // Lectura del botón (solo si estamos en entrenamiento activo)
  unsigned long tiempoAhora = millis();
  estadoBotonActual = digitalRead(botonPin);

  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW && (tiempoAhora - ultimoCambio) > debounceDelay) {
    ultimoCambio = tiempoAhora;
    unsigned long intervalo = tiempoAhora - tiempoAnterior;
    tiempoAnterior = tiempoAhora;

    compresionesTotales++;

    // Leer sensores
    int estadoSensor1 = digitalRead(sensor1Pin);
    int estadoSensor2 = digitalRead(sensor2Pin);
    bool fuerzaOK = false;

    if (estadoSensor1 == LOW && estadoSensor2 == LOW) {
      Serial.println("✅ Fuerza correcta");
      fuerzaCorrecta++;
      fuerzaOK = true;
    } else if (estadoSensor1 == LOW && estadoSensor2 == HIGH) {
      Serial.println("⚠️ Falta fuerza");
    } else if (estadoSensor1 == HIGH && estadoSensor2 == LOW) {
      Serial.println("⚠️ Demasiada fuerza");
    } else {
      Serial.println("🔴 Sin detección de fuerza");
    }

    // Evaluar ritmo
    if (intervalo >= intervaloMin && intervalo <= intervaloMax) {
      Serial.print("✅ Ritmo correcto - Intervalo: ");
      Serial.print(intervalo);
      Serial.println(" ms");
      ritmoCorrecto++;
    } else {
      Serial.print("⚠️ Ritmo incorrecto - Intervalo: ");
      Serial.print(intervalo);
      Serial.println(" ms");
    }
  }

  estadoBotonAnterior = estadoBotonActual;
}



