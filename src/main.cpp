/*
  DINAMÔMETRO - TESTE DE COMUNICAÇÃO ESP32 <-> ADS1256
  ------------------------------------------------------
  Etapa 1: comunicação SPI (ler registrador STATUS)
  Etapa 2: leitura bruta do canal diferencial AIN0(+)/AIN1(-)
  Etapa 3: conversão do valor bruto para tensão (volts)

  Pinagem usada (VSPI padrão do ESP32):
    ADS1256 SCLK  -> ESP32 GPIO18
    ADS1256 DIN   -> ESP32 GPIO23
    ADS1256 DOUT  -> ESP32 GPIO19
    ADS1256 CS    -> ESP32 GPIO5
    ADS1256 DRDY  -> ESP32 GPIO4
    ADS1256 PDWN  -> 5V (sempre ativo)
    ADS1256 5V/GND -> saída 5V do módulo USB-C
*/

#include <Arduino.h>
#include <SPI.h>

// ---------- Pinos ----------
#define PIN_CS    5
#define PIN_DRDY  4

// ---------- Comandos do ADS1256 ----------
#define CMD_WAKEUP   0x00
#define CMD_RDATA    0x01
#define CMD_RDATAC   0x03
#define CMD_SDATAC   0x0F
#define CMD_RREG     0x10
#define CMD_WREG     0x50
#define CMD_SELFCAL  0xF0
#define CMD_SYNC     0xFC
#define CMD_RESET    0xFE

// ---------- Registradores ----------
#define REG_STATUS  0x00
#define REG_MUX     0x01
#define REG_ADCON   0x02
#define REG_DRATE   0x03
#define REG_IO      0x04

// Referência de tensão do módulo (confirme no seu módulo; padrão costuma ser 2.5V)
const float VREF = 2.5;

SPIClass vspi(VSPI);
SPISettings ads1256Settings(1000000, MSBFIRST, SPI_MODE1); // 1MHz, modo 1 (CPOL=0, CPHA=1)

// ---------------------------------------------------------
// Funções de baixo nível
// ---------------------------------------------------------

void csLow()  { digitalWrite(PIN_CS, LOW); }
void csHigh() { digitalWrite(PIN_CS, HIGH); }

// Espera o ADC sinalizar dado pronto (DRDY vai para LOW)
void waitDRDY() {
  unsigned long start = millis();
  while (digitalRead(PIN_DRDY) == HIGH) {
    if (millis() - start > 1000) {
      Serial.println("ERRO: timeout esperando DRDY (verifique fiacao/alimentacao)");
      break;
    }
  }
}

// Escreve um valor em um registrador do ADS1256
void writeRegister(uint8_t reg, uint8_t value) {
  csLow();
  vspi.beginTransaction(ads1256Settings);
  vspi.transfer(CMD_WREG | reg);
  vspi.transfer(0x00);      // escrever apenas 1 registrador
  vspi.transfer(value);
  vspi.endTransaction();
  delayMicroseconds(10);
  csHigh();
}

// Lê um registrador do ADS1256
uint8_t readRegister(uint8_t reg) {
  uint8_t value;
  csLow();
  vspi.beginTransaction(ads1256Settings);
  vspi.transfer(CMD_RREG | reg);
  vspi.transfer(0x00);      // ler apenas 1 registrador
  delayMicroseconds(10);    // t6 delay exigido pelo datasheet
  value = vspi.transfer(0x00);
  vspi.endTransaction();
  csHigh();
  return value;
}

// Lê os 24 bits de dado do conversor (assume DRDY já em LOW)
long readData() {
  long value = 0;
  csLow();
  vspi.beginTransaction(ads1256Settings);
  vspi.transfer(CMD_RDATA);
  delayMicroseconds(10);    // t6 delay
  uint8_t b1 = vspi.transfer(0x00);
  uint8_t b2 = vspi.transfer(0x00);
  uint8_t b3 = vspi.transfer(0x00);
  vspi.endTransaction();
  csHigh();

  value = ((long)b1 << 16) | ((long)b2 << 8) | b3;

  // ajusta sinal (complemento de dois, 24 bits)
  if (value & 0x800000) {
    value |= 0xFF000000;
  }
  return value;
}

// ---------------------------------------------------------
// Setup do ADS1256
// ---------------------------------------------------------

void ads1256Init() {
  csHigh();
  delay(10);

  csLow();
  vspi.beginTransaction(ads1256Settings);
  vspi.transfer(CMD_RESET);
  vspi.endTransaction();
  csHigh();
  delay(50);

  csLow();
  vspi.beginTransaction(ads1256Settings);
  vspi.transfer(CMD_SDATAC); // garante que nao esta em modo de leitura continua
  vspi.endTransaction();
  csHigh();
  delayMicroseconds(10);

  // STATUS: habilita auto-calibracao (ACAL) e buffer de entrada (BUFEN)
  writeRegister(REG_STATUS, 0x06);

  // MUX: canal positivo AIN0, canal negativo AIN1 (par diferencial da celula)
  writeRegister(REG_MUX, 0x01);

  // ADCON: ganho = 1 (sem amplificacao extra), clock out desligado
  writeRegister(REG_ADCON, 0x00);

  // DRATE: 50 SPS - taxa moderada, boa para teste/estabilidade
  writeRegister(REG_DRATE, 0x63);

  csLow();
  vspi.beginTransaction(ads1256Settings);
  vspi.transfer(CMD_SELFCAL);
  vspi.endTransaction();
  csHigh();

  waitDRDY(); // aguarda a autocalibracao terminar
}

// ---------------------------------------------------------
// Arduino setup/loop
// ---------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== Teste ADS1256 - Dinamometro ===");

  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_DRDY, INPUT);
  csHigh();

  vspi.begin(); // usa pinos padrao do VSPI: SCK=18, MISO=19, MOSI=23

  ads1256Init();

  // ---- ETAPA 1: teste de comunicacao ----
  uint8_t status = readRegister(REG_STATUS);
  Serial.print("Registrador STATUS lido: 0x");
  Serial.println(status, HEX);
  Serial.println("(se nao for 0xFF nem 0x00 travado, a comunicacao SPI esta OK)");
  Serial.println();
}

void loop() {
  waitDRDY();
  long raw = readData();

  // ---- ETAPA 3: conversao para tensao ----
  // Full scale = +-VREF (com ganho 1), resolucao de 24 bits
  float voltage = (raw * VREF) / 8388608.0; // 2^23

  Serial.print("Bruto: ");
  Serial.print(raw);
  Serial.print("  |  Tensao: ");
  Serial.print(voltage, 6);
  Serial.println(" V");

  delay(200);
}