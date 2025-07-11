#include "DFRobotDFPlayerMini.h" //libreria
HardwareSerial mySerial(2); // crea un puerto serie en el UART2 del ESP32 (RX2/TX2), este puerto le sirve al df para comunicarse con el esp
DFRobotDFPlayerMini player; //asigno al dfplayer con el nombre de player


int currentTrack = 1; //el track con el que arranca es 1
const int botonPin = 33; //valor boton
const int sensorPin1 = 34;  // Sensor de poca fuerza
const int sensorPin2 = 35;  // Sensor de fuerza excesiva (nuevo)


unsigned long tiempoInicioTrack1 = 0; // guarada cuando empezo el track 1
const unsigned long tiempoRequerido = 3000; // tiempo que se tiene que mantener el boton
unsigned long tiempoPresionado = 0; //arranca con 0 y la idea es que llegue a los 3
unsigned long tiempoPresionadoParaReiniciar = 0; //lo mismo pero para reiniciar


int estadoBotonActual = HIGH; //arranca en high para dsp saber si se presiono
int estadoBotonAnterior = HIGH; // lo mismo


bool track1Reproducido = false; //para saber si ya se reprodujo el primer tema
bool esperandoFinTrack1 = false; // para detectar cuando termina el track 1 y pasar al 2
bool entrenamientoIniciado = false; // si el entrenamiento arranco  
bool entrenamientoFinalizado = false; // si el entrenamiento finalizo
bool track2YaIniciado = false; // si el track 2 ya arranco


unsigned long tiempoInicio = 0; //para contar los 30 segundos de entrenamiento  
unsigned long tiempoAnterior = 0; //para medir bpm
unsigned long ultimoCambio = 0; //para evitar rebotes
const unsigned long debounceDelay = 50; // delay para que no haya rebote


const unsigned long intervaloMin = 450; //bpm
const unsigned long intervaloMax = 650; //bpm


int compresionesTotales = 0;
int ritmoCorrecto = 0;
int fuerzaCorrecta = 0;
float PrimeraCompresion = 1;


bool presionado = false;
bool fuerzaDetectada = false;
bool fuerzaLiberada = false;
bool sensor1Detectado = false; // poca fuerza
bool sensor2Detectado = false; // fuerza excesiva


void setup() {
  pinMode(botonPin, INPUT_PULLUP);
  pinMode(sensorPin1, INPUT);
  pinMode(sensorPin2, INPUT);
  Serial.begin(9600);
  mySerial.begin(9600, SERIAL_8N1, 27, 26); // se inicializa el puerto UART (el de antes), en este caso el serial 2. Sirve para que el esp32 se comunique con el dfplayer.
  //SERIAL_8N1 es asi: 8 = hay 8 bits de datos, n = none: no se usa bit de paridad para deteccion de errores, 1 = 1 bit de stop, señal para marcar el fin de cada byte


  if (player.begin(mySerial)) { // si tiene exito el arranque del dfplayer (player es el df y mySerial es serial 2)
    player.volume(100); // volumen en 100
    Serial.println("👆 Mantené el botón 3 segundos para reproducir Track 1.");
  } else {
    Serial.println("❌ Error al iniciar DFPlayer.");
    while (true); // si no se puede iniciar el dfplayer se queda en un bucle
  }
}


void loop() {
  unsigned long ahora = millis();
  estadoBotonActual = digitalRead(botonPin);


  // ---------- INICIO TRACK 1 ----------
  if (!track1Reproducido) {
    if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW) {
      tiempoPresionado = ahora;
    }


    if (estadoBotonAnterior == LOW && estadoBotonActual == LOW && (ahora - tiempoPresionado >= tiempoRequerido)) {
      player.volume(100);
      Serial.println("▶️ Reproduciendo Track 1...");
      player.play(1);
      esperandoFinTrack1 = true;
      track1Reproducido = true;
      currentTrack = 1;
      tiempoInicioTrack1 = millis();
    }


    estadoBotonAnterior = estadoBotonActual;
    return;
  }


  // ---------- ESPERAR A QUE TERMINE TRACK 1 ----------
  if (esperandoFinTrack1 && player.available()) {
    if ((millis() - tiempoInicioTrack1 > 10000) && player.readType() == DFPlayerPlayFinished && currentTrack == 1) {
      esperandoFinTrack1 = false;
      currentTrack = 2;
    }
  }


  // ---------- INICIO ENTRENAMIENTO (TRACK 2) ----------
  if (currentTrack == 2 && !track2YaIniciado) {
    Serial.println("▶️ Iniciando Track 2 + entrenamiento...");
    player.volume(15);
    player.play(2);
    track2YaIniciado = true;


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
    sensor1Detectado = false;
    sensor2Detectado = false;


    Serial.println("🔁 Tenés 30 segundos para hacer compresiones.");
  }


  // ---------- FIN ENTRENAMIENTO + REINICIO ----------
  if (entrenamientoIniciado && !entrenamientoFinalizado && millis() - tiempoInicio >= 30000) {
    entrenamientoFinalizado = true;


    float compresionesTotales2 = compresionesTotales - PrimeraCompresion;
    float porcentajeRitmo = (compresionesTotales2 > 0) ? (ritmoCorrecto * 100.0 / compresionesTotales2) : 0;
    float porcentajeFuerza = (compresionesTotales2 > 0) ? (fuerzaCorrecta * 100.0 / compresionesTotales) : 0;
    float porcentajeGeneral = (porcentajeRitmo + porcentajeFuerza) / 2.0;


    Serial.println("⏹️ Tiempo finalizado.");
    Serial.print("🧮 Total de compresiones: "); Serial.println(compresionesTotales);
    Serial.print("🎯 Ritmo correcto: "); Serial.print(ritmoCorrecto); Serial.print(" ("); Serial.print(porcentajeRitmo, 1); Serial.println("%)");
    Serial.print("💪 Fuerza correcta: "); Serial.print(fuerzaCorrecta); Serial.print(" ("); Serial.print(porcentajeFuerza, 1); Serial.println("%)");
    Serial.print("📊 Evaluación general: "); Serial.print(porcentajeGeneral, 1); Serial.println("% de efectividad");


    Serial.println("✅ Entrenamiento terminado. Mantené el botón 3 segundos para reiniciar.");
  }


  if (entrenamientoFinalizado) {
    if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW) {
      tiempoPresionadoParaReiniciar = ahora;
    }


    if (estadoBotonAnterior == LOW && estadoBotonActual == LOW && (ahora - tiempoPresionadoParaReiniciar >= tiempoRequerido)) {
      Serial.println("🔄 Reiniciando sistema...");
      player.stop();


      currentTrack = 1;
      track1Reproducido = false;
      esperandoFinTrack1 = false;
      track2YaIniciado = false;
      entrenamientoIniciado = false;
      entrenamientoFinalizado = false;


      compresionesTotales = 0;
      ritmoCorrecto = 0;
      fuerzaCorrecta = 0;
      tiempoPresionado = 0;
      tiempoPresionadoParaReiniciar = 0;
      estadoBotonAnterior = HIGH;


      Serial.println("🟢 Sistema reiniciado. Mantené el botón 3 segundos para reproducir Track 1.");
      return;
    }


    estadoBotonAnterior = estadoBotonActual;
    return;
  }


  // ---------- MEDIR COMPRESIONES ----------
  if (!entrenamientoIniciado || entrenamientoFinalizado) return;


  unsigned long tiempoAhora = millis();
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


      // ----- Evaluar tipo de fuerza -----
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


      // ----- Evaluar ritmo -----
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

