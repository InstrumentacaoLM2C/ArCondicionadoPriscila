// ====================================================================

/*
INSTRUÇÕES:

. É necessário calibrar a posição dos infravermelhos, sempre aponte eles diretamente para os sensores do arcondicionado

Caso pare de funcionar: 
. verifique se os infravermelhos estão atuando, se houve algum dano nas conexões, ou se queimaram,
. verifique se o RTC precisa da troca da bateria e verifique seus terminais,
. troque o cabo do Arduino,
. teste a continuidade das trilhas da placa, que não podem se cruzar, para procurar por curto-circuitos.
. limpe a placa com alcool isopropilico para retirar possiveis corrosões devido a oxidação

Para obter pulsos:
 . os pulsos foram obtidos a partir de um controle universal de ar-condcionado, capaz de atuar o LG e o TCL
 . os pulsos do LG foram obtidos a partir do exemplo "sendLGAirConditioner" na biblioteca IRremote
 . a biblioteca utilizada foi a IRremote, utilizando os exemplos "receiveDemo" para capturar pulsos e "sendDemo" para enviar
 . os controles originais produzem pulsos muito complexos para serem captados com um receptor IR simples, priorizar sempre o controle universal
 . capture pulsos do controle em distâncias diferentes e teste qual atua melhor o ar-condicionado fazendo um código de emissão simples

. A temperatura não deve ultrapassar os 24 graus, idealmente ficando em 22, para não correr danos a equipamentos que esquentam muito, como o computador

. O circuito consiste em dois leds infravermelhos conectados nas portas digitais 2 e 3.
. O display LCD e o RTC, utilizam comunicação I2C, e se conectam nas portas analogicas A4 e A5
. A porta A4 é o SDA, a porta A5 é o SCL
. Utilizamos um Arduino Nano
*/


//Autor: João Gabriel



// CONTROLE DUAL AC - TURNOS POR TEMPO (LÓGICA CORRIGIDA)
// ====================================================================
#include <Arduino.h>
#include <IRremote.hpp>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

// --------------------------------------------------------------------
// HARDWARE
// --------------------------------------------------------------------
const int IR_PIN_LG  = 2;
const int IR_PIN_TCL = 3;
const int LCD_ADDR  = 0x27;

// --------------------------------------------------------------------
// CONFIGURAÇÕES
// --------------------------------------------------------------------
const float TEMPERATURA_LIMITE = 24.0;

// >>> INTERVALO DE TURNO <<<
const uint32_t INTERVALO_TURNO = 2 * 3600UL; // 2 horas


// para teste rápido: 15 segundos -> 15
//const uint32_t INTERVALO_TURNO = 15; // 2 horas


// --------------------------------------------------------------------
// DADOS IR (CONFIRMADOS)
// --------------------------------------------------------------------

// TCL ON
uint32_t TCL_DATA_ON[]  = {0x126CB23, 0x9032400, 0x0, 0x4500};

// TCL OFF
uint32_t TCL_DATA_OFF[] = {0x126CB23, 0x9032000, 0x0, 0x4100};

// LG (LG2)
const uint8_t  LG_ADDR    = 0x88;
const uint16_t LG_CMD_ON  = 0x0072;
const uint16_t LG_CMD_OFF = 0xC005;

// --------------------------------------------------------------------
// OBJETOS
// --------------------------------------------------------------------
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
RTC_DS3231 rtc;

// --------------------------------------------------------------------
// VARIÁVEIS
// --------------------------------------------------------------------
bool  isLGTurn       = true;
bool  emergencyMode = false;
float currentTemp   = 0.0;

uint32_t ultimoTurnoEpoch = 0;

// --------------------------------------------------------------------
// FUNÇÕES AUXILIARES
// --------------------------------------------------------------------
float readTemperature() {
  return rtc.getTemperature();
}

void printTwoDigits(int n) {
  if (n < 10) lcd.print('0');
  lcd.print(n);
}

// --------------------------------------------------------------------
// CONTROLE DE TURNO (CORRETO)
// --------------------------------------------------------------------
void iniciarTurno() {
  ultimoTurnoEpoch = rtc.now().unixtime();
}

bool tempoDeTrocarTurno() {
  uint32_t agora = rtc.now().unixtime();
  return (agora - ultimoTurnoEpoch) >= INTERVALO_TURNO;
}

// --------------------------------------------------------------------
// ENVIO IR
// --------------------------------------------------------------------
void enviarLG(bool ligar) {
  IrSender.setSendPin(IR_PIN_LG);
  Serial.print(F("[LG] "));
  Serial.println(ligar ? F("ON") : F("OFF"));

  if (ligar) IrSender.sendLG2(LG_ADDR, LG_CMD_ON, 0);
  else       IrSender.sendLG2(LG_ADDR, LG_CMD_OFF, 0);
}

void enviarTCL(bool ligar) {
  IrSender.setSendPin(IR_PIN_TCL);
  Serial.print(F("[TCL] "));
  Serial.println(ligar ? F("ON") : F("OFF"));

  if (ligar) {
    IrSender.sendPulseDistanceWidthFromArray(
      38,
      3400, 1550,
      350, 1250,
      350, 400,
      TCL_DATA_ON,
      112,
      PROTOCOL_IS_LSB_FIRST,
      0,
      1
    );
  } else {
    IrSender.sendPulseDistanceWidthFromArray(
      38,
      3400, 1500,
      350, 1250,
      350, 400,
      TCL_DATA_OFF,
      112,
      PROTOCOL_IS_LSB_FIRST,
      0,
      1
    );
  }
}

// --------------------------------------------------------------------
// LCD
// --------------------------------------------------------------------
void updateLCD() {
  lcd.setCursor(0, 0);

  if (emergencyMode) {
    lcd.print("! EMERGENCIA ! ");
  } else {
    lcd.print(isLGTurn ? "LG [ON] " : "TCL[ON] ");

    uint32_t agora = rtc.now().unixtime();
    uint32_t restante = (ultimoTurnoEpoch + INTERVALO_TURNO - agora);

    lcd.setCursor(8, 0);
    lcd.print("Prx:");
    printTwoDigits(restante / 60); // minutos
    lcd.print("m");
  }

  lcd.setCursor(0, 1);
  lcd.print(currentTemp, 1);
  lcd.print((char)223);
  lcd.print("C ");

  DateTime now = rtc.now();
  printTwoDigits(now.hour());
  lcd.print(':');
  printTwoDigits(now.minute());
}

// --------------------------------------------------------------------
// SETUP
// --------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(IR_PIN_LG, OUTPUT);
  pinMode(IR_PIN_TCL, OUTPUT);

  IrSender.begin(IR_PIN_LG);

  lcd.init();
  lcd.backlight();

  if (!rtc.begin()) {
    lcd.print("ERRO RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lcd.clear();
  lcd.print("INICIANDO...");
  delay(1000);

  // --- INÍCIO DO SISTEMA ---
  isLGTurn = true;   // começa com LG
  iniciarTurno();

  enviarLG(true);
  delay(1000);
  enviarTCL(false);

  lcd.clear();
}

// --------------------------------------------------------------------
// LOOP
// --------------------------------------------------------------------
void loop() {
  currentTemp = readTemperature();

  // --- EMERGÊNCIA ---
  if (currentTemp > TEMPERATURA_LIMITE && !emergencyMode) {
    emergencyMode = true;
    lcd.clear();
    lcd.print("ALERTA TEMP!");
    enviarLG(true);
    delay(1000);
    enviarTCL(true);
    lcd.clear();
  }

  // --- SAÍDA DA EMERGÊNCIA ---
  if (emergencyMode && currentTemp <= TEMPERATURA_LIMITE - 1.0) {
    emergencyMode = false;
    lcd.clear();
    lcd.print("NORMALIZADO");

    iniciarTurno();

    if (isLGTurn) {
      enviarLG(true);
      delay(1000);
      enviarTCL(false);
    } else {
      enviarTCL(true);
      delay(1000);
      enviarLG(false);
    }
    lcd.clear();
  }

  // --- TROCA DE TURNO ---
  if (!emergencyMode && tempoDeTrocarTurno()) {

    isLGTurn = !isLGTurn;
    iniciarTurno();

    lcd.clear();
    lcd.print("TROCA TURNO");

    if (isLGTurn) {
      enviarLG(true);
      delay(2000);
      enviarTCL(false);
    } else {
      enviarTCL(true);
      delay(2000);
      enviarLG(false);
    }
    lcd.clear();
  }

  static unsigned long lastLCD = 0;
  if (millis() - lastLCD > 500) {
    updateLCD();
    lastLCD = millis();
  }
}
