const int botonPin = 35;
const int sensorPin = 34;

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

// Banderas internas
bool presionado = false;
bool fuerzaDetectada = false;
bool fuerzaLiberada = false;

void setup() {
  pinMode(botonPin, INPUT_PULLUP);
  pinMode(sensorPin, INPUT);
  Serial.begin(115200);
  Serial.println("📢 Escribí 'ya' en el Monitor Serie para comenzar el entrenamiento de 30 segundos.");
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
        fuerzaDetectada = false;
        fuerzaLiberada = false;
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

  
  unsigned long tiempoAhora = millis();
  estadoBotonActual = digitalRead(botonPin);
  int estadoSensor = digitalRead(sensorPin);  

  
  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW && (tiempoAhora - ultimoCambio) > debounceDelay) {
    ultimoCambio = tiempoAhora;
    presionado = true;
    fuerzaDetectada = false;
    fuerzaLiberada = false;
    //Serial.println("🔽 Botón presionado");
  }

  
  if (presionado) {
    if (!fuerzaDetectada && estadoSensor == LOW) {
      fuerzaDetectada = true;
      //Serial.println("💪 Fuerza detectada");
    }

    if (fuerzaDetectada && !fuerzaLiberada && estadoSensor == HIGH) {
      fuerzaLiberada = true;
      //Serial.println("🛑 Fuerza liberada");
    }

    
    if (estadoBotonAnterior == LOW && estadoBotonActual == HIGH && (tiempoAhora - ultimoCambio) > debounceDelay) {
      ultimoCambio = tiempoAhora;
      presionado = false;
      compresionesTotales++;

      //Serial.println("🔼 Botón soltado");

      
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
