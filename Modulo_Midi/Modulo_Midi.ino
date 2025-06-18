#include "DFRobotDFPlayerMini.h"


HardwareSerial mySerial(2); // Serial2 para ESP32
DFRobotDFPlayerMini player;


int currentTrack = 1;
const int totalTracks = 10;


bool trackEnded = false; // Flag para controlar evento único
unsigned long lastChangeTime = 0;
const unsigned long changeDelay = 500; // ms de espera entre pistas


void setup() {
  Serial.begin(9600);
  mySerial.begin(9600, SERIAL_8N1, 27, 26); // RX=27, TX=26


  if (player.begin(mySerial)) {
    Serial.println("DFPlayer listo");
    player.volume(100);
    player.play(currentTrack);
    Serial.print("Reproduciendo archivo ");
    Serial.println(currentTrack);
  } else {
    Serial.println("Error con DFPlayer");
    while (true); // Para ejecución si no conecta
  }
}


void loop() {
  if (player.available()) {
    int type = player.readType();


    if (type == DFPlayerPlayFinished) {
      Serial.print("Archivo ");
      Serial.print(currentTrack);
      Serial.println(" finalizado.");


      trackEnded = true;
      lastChangeTime = millis();
    }
  }


  // Espera a que pase el delay y luego cambia a la siguiente pista
  if (trackEnded && (millis() - lastChangeTime >= changeDelay)) {
    currentTrack++;
    if (currentTrack > totalTracks) {
      currentTrack = 1;
    }
    player.play(currentTrack);
    Serial.print("Reproduciendo archivo ");
    Serial.println(currentTrack);


    trackEnded = false; // Resetea el flag para próximo evento
  }
}



