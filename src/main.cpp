#include <Arduino.h>
#include <HX711.h>

// Defina os pinos DT e SCK
#define HX711_DT D2
#define HX711_SCK D1

HX711 scale;

void setup() {
  Serial.begin(115200);
  scale.begin(HX711_DT, HX711_SCK);
  scale.set_scale();  // ajuste depois com calibração
  scale.tare();       // zera a balança
  Serial.println("Sensor inicializado");
}

void loop() {
  if (scale.is_ready()) {
    long leitura = scale.get_units(10); // média de 10 leituras
    Serial.print("Peso: ");
    Serial.print(leitura);
    Serial.println(" kg");
  } else {
    Serial.println("Erro: HX711 não pronto");
  }
  delay(500);
}
