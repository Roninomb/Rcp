const int botonPin = 34;  // Pin del botón


int estadoBotonActual = HIGH;
int estadoBotonAnterior = HIGH;


unsigned long tiempoAnterior = 0;
unsigned long tiempoActual = 0;
unsigned long ultimoCambio = 0;


const unsigned long intervaloMin = 400;  // milisegundos -> 120 bpm
const unsigned long intervaloMax = 700;  // milisegundos -> 100 bpm
const unsigned long debounceDelay = 50;  // para evitar rebotes


int contador = 0;


void setup() {
  pinMode(botonPin, INPUT_PULLUP);
  Serial.begin(115200);

}


void loop() {
  estadoBotonActual = digitalRead(botonPin);
  unsigned long tiempoAhora = millis();


  // Detectar flanco de bajada + esperar a que pase debounceDelay
  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW && (tiempoAhora - ultimoCambio) > debounceDelay) {
    ultimoCambio = tiempoAhora;  // para debounce


    tiempoActual = tiempoAhora;
    unsigned long intervalo = tiempoActual - tiempoAnterior;


    if (intervalo >= intervaloMin && intervalo <= intervaloMax) {
      contador++;
      Serial.print("✅ Pulso válido #");
      Serial.print(contador);
      Serial.print(" - Intervalo: ");
      Serial.print(intervalo);
      Serial.println(" ms");
    } else {
      Serial.print("⚠️ Pulso fuera de ritmo - Intervalo: ");
      Serial.print(intervalo);
      Serial.println(" ms");
    }


    tiempoAnterior = tiempoActual;
  }


  estadoBotonAnterior = estadoBotonActual;
}



