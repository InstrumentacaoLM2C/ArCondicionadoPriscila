// ====================================================================
// CONTROLE DUAL AC - VERSÃO RELÓGIO (TURNOS DE 6H)
// ====================================================================
#include <Arduino.h>
#include <IRremote.hpp>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

// --- HARDWARE ---
const int IR_PIN_LG = 3;
const int IR_PIN_TCL = 5;
const int LCD_ADDR = 0x27;

// --- CONFIGURAÇÕES ---
const float TEMPERATURA_LIMITE = 28.0;

// ====================================================================
// DADOS DOS CONTROLES IR
// ====================================================================

// --- 1. TCL (PulseDistance - 112 bits) ---
uint32_t TCL_DATA_ON[]  = {0x126CB23, 0x9032400, 0x0, 0x4500};
uint32_t TCL_DATA_OFF[] = {0x126CB23, 0x9032000, 0x0, 0x4100};

// --- 2. LG (Protocolo LG2) ---
const uint8_t LG_ADDR = 0x88;
const uint16_t LG_CMD_ON  = 0x0072;
const uint16_t LG_CMD_OFF = 0xC005;

// ====================================================================
// OBJETOS E VARIÁVEIS
// ====================================================================
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
RTC_DS3231 rtc;

bool isLGTurn = true; 
float currentTemperature = 0.0;
bool emergencyMode = false;

// ====================================================================
// FUNÇÕES AUXILIARES
// ====================================================================
float readTemperature() { return rtc.getTemperature(); }
void printTwoDigits(int number) { if(number < 10) lcd.print("0"); lcd.print(number); }

// ====================================================================
// ENVIO IR (MANTIDO IGUAL)
// ====================================================================

void enviarLG(bool ligar) {
  IrSender.setSendPin(IR_PIN_LG); 
  Serial.print(F("[LG] Enviando: ")); Serial.println(ligar ? F("ON") : F("OFF"));
  if (ligar) IrSender.sendLG2(LG_ADDR, LG_CMD_ON, 0); 
  else       IrSender.sendLG2(LG_ADDR, LG_CMD_OFF, 0);
}

void enviarTCL(bool ligar) {
  IrSender.setSendPin(IR_PIN_TCL);
  Serial.print(F("[TCL] Enviando: ")); Serial.println(ligar ? F("PULSE ON") : F("PULSE OFF"));
  
  if (ligar) {
      IrSender.sendPulseDistanceWidthFromArray(38, 3350, 1550, 350, 1300, 350, 400, &TCL_DATA_ON[0], 112, PROTOCOL_IS_LSB_FIRST, 0, 0);
  } else {
      IrSender.sendPulseDistanceWidthFromArray(38, 3350, 1550, 350, 1250, 350, 400, &TCL_DATA_OFF[0], 112, PROTOCOL_IS_LSB_FIRST, 0, 0);
  }
}

// ====================================================================
// LÓGICA DE TURNOS (6 em 6 Horas)
// ====================================================================
// Retorna TRUE se for horário do LG, FALSE se for horário do TCL
bool verificarHorarioLG() {
    DateTime now = rtc.now();
    int hora = now.hour();

    // DEFINIÇÃO DOS TURNOS:
    // 00:00 as 05:59 -> LG
    // 06:00 as 11:59 -> TCL
    // 12:00 as 17:59 -> LG
    // 18:00 as 23:59 -> TCL
    
    if ( (hora >= 0 && hora < 6) || (hora >= 12 && hora < 18) ) {
        return true; // Turno LG
    } else {
        return false; // Turno TCL
    }
}

// ====================================================================
// DISPLAY ATUALIZADO
// ====================================================================
void updateLCD() {
  lcd.setCursor(0, 0);
  if (emergencyMode) {
    lcd.print("! EMERGENCIA !  ");
  } else {
    // Linha 0: Status e Próxima Troca
    lcd.print(isLGTurn ? "LG [ON] " : "TCL[ON] ");
    
    // Calcula próxima troca
    int horaAtual = rtc.now().hour();
    int proxTroca = 0;
    if (horaAtual < 6) proxTroca = 6;
    else if (horaAtual < 12) proxTroca = 12;
    else if (horaAtual < 18) proxTroca = 18;
    else proxTroca = 0; // Meia noite

    lcd.setCursor(8, 0); 
    lcd.print("Prx:"); 
    printTwoDigits(proxTroca);
    lcd.print("h");
  }

  // Linha 1: Temperatura e Hora Real
  lcd.setCursor(0, 1);
  lcd.print(currentTemperature, 1); lcd.print((char)223); lcd.print("C    ");
  
  DateTime now = rtc.now();
  printTwoDigits(now.hour()); lcd.print(':'); printTwoDigits(now.minute());
}

// ====================================================================
// SETUP
// ====================================================================
void setup() {
  Serial.begin(115200);
  
  pinMode(IR_PIN_LG, OUTPUT);
  pinMode(IR_PIN_TCL, OUTPUT);
  IrSender.begin(IR_PIN_LG); 

  lcd.init(); lcd.backlight();
  if (!rtc.begin()) {
      lcd.print("ERRO RTC!");
      while(1);
  }

  if (rtc.lostPower()) {
      // Se a bateria do RTC acabou, ajusta para data de compilação
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // --- DEFINIÇÃO INICIAL DO TURNO ---
  // Verifica que horas são AGORA para ligar o ar correto imediatamente
  lcd.clear(); lcd.print("VERIFICANDO"); lcd.setCursor(0,1); lcd.print("HORARIO...");
  delay(1000);

  isLGTurn = verificarHorarioLG(); // Define o turno baseado na hora real

  if (isLGTurn) {
      Serial.println(F("Inicio: Turno LG detectado."));
      enviarLG(true); 
      delay(1000);
      enviarTCL(false);
  } else {
      Serial.println(F("Inicio: Turno TCL detectado."));
      enviarTCL(true);
      delay(1000);
      enviarLG(false);
  }
  
  lcd.clear();
}

// ====================================================================
// LOOP PRINCIPAL
// ====================================================================
void loop() {
  currentTemperature = readTemperature();

  // 1. EMERGÊNCIA (> 28C)
  if (currentTemperature > TEMPERATURA_LIMITE) {
    if (!emergencyMode) {
      emergencyMode = true;
      lcd.clear(); lcd.print("ALERTA TEMP!");
      enviarLG(true); delay(1000); enviarTCL(true);
      lcd.clear();
    }
  }
  // 2. NORMALIZADO
  else if (emergencyMode && currentTemperature <= (TEMPERATURA_LIMITE - 1.0)) {
    emergencyMode = false;
    lcd.clear(); lcd.print("NORMALIZADO");
    
    // Ao sair da emergência, verifica qual turno deveria estar ativo pelo relógio
    isLGTurn = verificarHorarioLG();
    
    if (isLGTurn) { enviarLG(true); delay(1000); enviarTCL(false); } 
    else          { enviarTCL(true); delay(1000); enviarLG(false); }
    
    lcd.clear();
  }

  // 3. VERIFICAÇÃO DE TROCA DE TURNO (AGORA POR HORÁRIO)
  if (!emergencyMode) {
      bool turnoCorretoPeloRelogio = verificarHorarioLG();

      // Se o turno calculado pelo relógio for diferente do turno que está ativo agora
      if (turnoCorretoPeloRelogio != isLGTurn) {
          Serial.println(F("\n[TURNO] Hora de trocar!"));
          
          isLGTurn = turnoCorretoPeloRelogio; // Atualiza a variável

          lcd.clear();
          lcd.setCursor(0, 0); lcd.print(" TROCA DE TURNO ");
          lcd.setCursor(0, 1); lcd.print(isLGTurn ? "-> LIGANDO LG" : "-> LIGANDO TCL");

          if (isLGTurn) {
            // Entra LG (00h ou 12h)
            enviarLG(true); delay(2000); enviarTCL(false);
          } else {
            // Entra TCL (06h ou 18h)
            enviarTCL(true); delay(2000); enviarLG(false);
          }
          lcd.clear();
      }
  }

  static unsigned long lastLCD = 0;
  if (millis() - lastLCD > 500) { updateLCD(); lastLCD = millis(); }
}