#include "DFRobotDFPlayerMini.h" //libreria
HardwareSerial mySerial(2); // crea un puerto serie en el UART2 del ESP32 (RX2/TX2), este puerto le sirve al df para comunicarse con el esp
DFRobotDFPlayerMini player; //asigno al dfplayer con el nombre de player

int currentTrack = 1; //el track con el que arranca es 1 
const int botonPin = 33; //valor boton
const int sensorPin = 34; //valor sensor

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

void setup() {
  pinMode(botonPin, INPUT_PULLUP);
  pinMode(sensorPin, INPUT);
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
  unsigned long ahora = millis(); // guarda el tiempo desde que se encendio el esp en ahora
  estadoBotonActual = digitalRead(botonPin); // lee boton

  // ---------- INICIO TRACK 1 ----------
  if (!track1Reproducido) { // si todavia no se reprodujo el track 1
    if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW) { // detecta el momento en el que fue presionado
      tiempoPresionado = ahora; // mientras es presionado lo guarda en tiempopresionado
    }

if (estadoBotonAnterior == LOW && estadoBotonActual == LOW && (ahora - tiempoPresionado >= tiempoRequerido)) { // si el boton se mantiene presionado durante 3 segundos
  player.volume(100);
  Serial.println("▶️ Reproduciendo Track 1...");
  player.play(1); // se reproduce el track 1
  esperandoFinTrack1 = true; // se activa para que dsp se sepa que se debe esperar a que termine el track 1
  track1Reproducido = true; // marca que el track 1 ya inicio
  currentTrack = 1; // el track actual es 1 
  tiempoInicioTrack1 = millis(); // guarda el momento en que comenzó la reproducción del track 1, para evitar que detecte su fin antes de tiempo.
}

    estadoBotonAnterior = estadoBotonActual; // Guarda el estado actual del botón (si está presionado o no) como "anterior" para poder compararlo en el próximo ciclo del loop().
    return; // Corta la ejecución del loop() en ese punto y salta al próximo ciclo. 
  }

  // ---------- ESPERAR A QUE TERMINE TRACK 1 ----------
if (esperandoFinTrack1 && player.available()) { // si estamos esperando que termine el track 1 y hay datos disponibles del df
  if ((millis() - tiempoInicioTrack1 > 10000) && player.readType() == DFPlayerPlayFinished && currentTrack == 1) { // si pasaron 10 segundos desde que empezo el track 1, 
  //y el dfplayer mando un mensaje diciendo que termino el track actual, y el track que termino es el 1
    esperandoFinTrack1 = false; // no estamos mas esperando el fin del track 
    currentTrack = 2; // pasa a ser el track 2 
  }
}

  // ---------- INICIO ENTRENAMIENTO (TRACK 2) ----------
  if (currentTrack == 2 && !track2YaIniciado) { // si ya se paso al track 2 y todavio no se inicio
    Serial.println("▶️ Iniciando Track 2 + entrenamiento...");
    player.volume(15); //le bajo el volumen pq baby shark esta re fuerte
    player.play(2); // comienza el track 2
    track2YaIniciado = true; // ya comenzo el track 2

    entrenamientoIniciado = true; // arranco
    entrenamientoFinalizado = false; //todavia no finalizo
    compresionesTotales = 0; //se marca todo en 0 para reiniciar
    ritmoCorrecto = 0;
    fuerzaCorrecta = 0;
    tiempoAnterior = 0;
    ultimoCambio = 0;
    tiempoInicio = millis(); // marca los 30 segundos
    presionado = false; // todo en false
    fuerzaDetectada = false;
    fuerzaLiberada = false;

    Serial.println("🔁 Tenés 30 segundos para hacer compresiones.");
  }

  // ---------- FIN ENTRENAMIENTO + REINICIO ----------
  if (entrenamientoIniciado && !entrenamientoFinalizado && millis() - tiempoInicio >= 30000) { // si el entrenamiento ya arranco y no termino, pero ya pasaron 30 segundos
    entrenamientoFinalizado = true; // finalizo el entrenamiento

    float compresionesTotales2 = compresionesTotales - PrimeraCompresion; // para que no detecte la primera compresion
    float porcentajeRitmo = (compresionesTotales2 > 0) ? (ritmoCorrecto * 100.0 / compresionesTotales2) : 0; // Si hubo compresiones, calculamos el porcentaje de compresiones que tuvieron el ritmo correcto. 
    //Si no hubo compresiones, queda en 0 para evitar división por cero.
    float porcentajeFuerza = (compresionesTotales2 > 0) ? (fuerzaCorrecta * 100.0 / compresionesTotales) : 0; // lo mism
    float porcentajeGeneral = (porcentajeRitmo + porcentajeFuerza) / 2.0; // promedio entre ritmo y fuerza

    Serial.println("⏹️ Tiempo finalizado.");
    Serial.print("🧮 Total de compresiones: "); Serial.println(compresionesTotales);
    Serial.print("🎯 Ritmo correcto: "); Serial.print(ritmoCorrecto); Serial.print(" ("); Serial.print(porcentajeRitmo, 1); Serial.println("%)");
    Serial.print("💪 Fuerza correcta: "); Serial.print(fuerzaCorrecta); Serial.print(" ("); Serial.print(porcentajeFuerza, 1); Serial.println("%)");
    Serial.print("📊 Evaluación general: "); Serial.print(porcentajeGeneral, 1); Serial.println("% de efectividad");

    Serial.println("✅ Entrenamiento terminado. Mantené el botón 3 segundos para reiniciar.");
  }

  if (entrenamientoFinalizado) { // si el entrenamiento termino
    if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW) { //detecta el momento en el que fue presionado
      tiempoPresionadoParaReiniciar = ahora; //lo guarda
    }

    if (estadoBotonAnterior == LOW && estadoBotonActual == LOW && (ahora - tiempoPresionadoParaReiniciar >= tiempoRequerido)) { // si el boton sigue presionado y pasaron 3 segundos
      Serial.println("🔄 Reiniciando sistema...");
      player.stop(); // se para cualquier reproducción

      // se reseta por completo el sistema
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
      return; // return sale de la función loop() y espera al próximo ciclo.
    }

    estadoBotonAnterior = estadoBotonActual;
    return;
  }

  // ---------- MEDIR COMPRESIONES ----------
  if (!entrenamientoIniciado || entrenamientoFinalizado) return; // si la funcion no arranco o ya finalizo el loop termina para que no se midan compresiones 

  unsigned long tiempoAhora = millis(); // guarda el tiempo
  int estadoSensor = digitalRead(sensorPin); // lee para detectar fuerza

  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW && (tiempoAhora - ultimoCambio) > debounceDelay) { // detecta el boton y asegura que pase mas tiempo que el delay anti rebote
    ultimoCambio = tiempoAhora;
    presionado = true; // compresion
    fuerzaDetectada = false; // estan en false para que cuando sea presionado detecte
    fuerzaLiberada = false;
  }

  if (presionado) { // si detecta presionado
    if (!fuerzaDetectada && estadoSensor == LOW) fuerzaDetectada = true; // si no se detectó fuerza y el sensor indica LOW (se presionó con fuerza), marca que se detectó fuerza
    if (fuerzaDetectada && !fuerzaLiberada && estadoSensor == HIGH) fuerzaLiberada = true; // si ya se detectó la fuerza pero aún no se detectó que la fuerza se liberó, y el sensor ahora indica high, se marca que la fuerza fue liberada

    if (estadoBotonAnterior == LOW && estadoBotonActual == HIGH && (tiempoAhora - ultimoCambio) > debounceDelay) { // si el botón pasó de estar presionado a estar suelto, y además pasó suficiente tiempo desde el último cambio para evitar rebotes.
      ultimoCambio = tiempoAhora; // actualiza la última vez que se detectó un cambio de estado del botón
      presionado = false; // marca que la compresion termino
      compresionesTotales++; // incrementa el contador de compresiones

      if (fuerzaDetectada && fuerzaLiberada) { // si se detectó fuerza y liberación 
        fuerzaCorrecta++; // incrementa el contador de fuerza correcta
        Serial.println("✅ Fuerza correcta");
      } else {
        Serial.println("⚠️ Fuerza insuficiente");
      }

      unsigned long intervalo = tiempoAhora - tiempoAnterior; // bpm
      if (intervalo >= intervaloMin && intervalo <= intervaloMax) {  // si el intevalo esta en el ritmo correcto
        ritmoCorrecto++; // se le suma al contador de ritmo correcto
        Serial.print("✅ Ritmo correcto: ");
      } else {
        Serial.print("⚠️ Ritmo incorrecto: ");
      }

      Serial.print(intervalo);
      Serial.println(" ms");

      tiempoAnterior = tiempoAhora; // actualiza el tiempo de la ultima compresion para comparar con la siguiente
    }
  }

  estadoBotonAnterior = estadoBotonActual;
}
