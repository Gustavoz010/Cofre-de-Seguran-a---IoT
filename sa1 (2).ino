
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <Servo.h>
#include <string.h>

// COMPONENTES
const int buzzer = 12;
const int motor = 9;
const int led = 8;
const int ledVermelho = 11;
const int sensorLDR = A0;
const int botaoAlarme = 13;
const int laser = 10;

// SENSOR ULTRASSONICO HC-SR04
const int trig = 0;
const int echo = 1;
const int distanciaLimite = 20;

bool presencaDetectada = false;

// SERVO
Servo servo;
const int servoFechado = 0;
const int servoAberto = 90;

// SENSOR LDR
int leituraLDR = 0;
float tensaoLDR = 0.0;
const int limite = 600;

bool sensorAguardandoLiberacao = false;

unsigned long tempoLeituraLDR = 0;

// LCD I2C
hd44780_I2Cexp lcd;

// TECLADO
const byte linhas = 4;
const byte colunas = 4;

char teclas[4][4] = {
  {'1','2','3','X'},
  {'4','5','6','X'},
  {'7','8','9','X'},
  {'*','0','#','X'}
};

byte pinosLinhas[4] = {2, 3, 4, 5};
byte pinosColunas[4] = {6, 7, A1, A3};

// SENHA
char senha[5];
int posicao = 0;
int tentativasErradas = 0;

// ESTADOS
bool cofreAberto = false;
bool invasao = false;
bool alarmeAtivo = false;

unsigned long tempoAlarme = 0;
int etapaAlarme = 0;

// LED VERMELHO
unsigned long tempoPisca = 0;
bool estadoLedVermelho = false;

// LIMPAR SENHA
void limparBufferSenha() {
  posicao = 0;

  for (int i = 0; i < 5; i++) {
    senha[i] = '\0';
  }
}

// TELA INICIAL
void mostrarTelaInicial() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Digite a senha:");

  limparBufferSenha();

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
}

// APAGAR SENHA
void apagarSenha() {
  limparBufferSenha();

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
}

// SOM DA TECLA
void bipTecla() {
  tone(buzzer, 1200);
  delay(60);
  noTone(buzzer);
}

// MEDIR DISTANCIA DO ULTRASSONICO
float medirDistancia() {

  digitalWrite(trig, LOW);
  delayMicroseconds(2);

  digitalWrite(trig, HIGH);
  delayMicroseconds(10);

  digitalWrite(trig, LOW);

  unsigned long duracao = pulseIn(echo, HIGH, 25000);

  if (duracao == 0) {
    return 0;
  }

  float distancia = duracao * 0.0343 / 2;

  return distancia;
}

// SOM DO ALARME
void tocarAlarme() {
  unsigned long agora = millis();

  if (agora - tempoAlarme >= 200) {
    tempoAlarme = agora;
    etapaAlarme++;

    if (etapaAlarme > 3) {
      etapaAlarme = 0;
    }

    if (etapaAlarme == 0) tone(buzzer, 1000);
    if (etapaAlarme == 1) tone(buzzer, 1500);
    if (etapaAlarme == 2) tone(buzzer, 2000);
    if (etapaAlarme == 3) tone(buzzer, 1500);
  }
}

// CONTROLAR LEDS
void controlarLeds() {

  if (alarmeAtivo) {

    digitalWrite(led, LOW);

    if (millis() - tempoPisca >= 500) {
      tempoPisca = millis();

      estadoLedVermelho = !estadoLedVermelho;

      digitalWrite(ledVermelho, estadoLedVermelho);
    }

  } else {

    digitalWrite(ledVermelho, LOW);
    estadoLedVermelho = false;

    // LED VERDE CONTROLADO PELO ULTRASSONICO
    if (presencaDetectada && !cofreAberto) {
      digitalWrite(led, HIGH);
    } else {
      digitalWrite(led, LOW);
    }
  }
}

// DISPARAR ALARME
void dispararAlarme() {
  if (alarmeAtivo) return;

  alarmeAtivo = true;
  invasao = true;

  servo.write(servoFechado);
  cofreAberto = false;

  digitalWrite(laser, HIGH);

  digitalWrite(led, LOW);

  tempoPisca = millis();
  estadoLedVermelho = false;
  digitalWrite(ledVermelho, LOW);

  tempoAlarme = millis();
  etapaAlarme = 0;

  limparBufferSenha();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ALARME!");

  lcd.setCursor(0, 1);
  lcd.print("Cofre trancado");
}

// VERIFICAR SENHA
void verificarSenha() {
  senha[4] = '\0';

  if (strcmp(senha, "2026") == 0) {

    tentativasErradas = 0;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SENHA CORRETA!");

    if (!cofreAberto) {

      servo.write(servoAberto);
      cofreAberto = true;

      digitalWrite(laser, LOW);

      lcd.setCursor(0, 1);
      lcd.print("Abrindo cofre");

    } else {

      servo.write(servoFechado);
      cofreAberto = false;

      digitalWrite(laser, HIGH);

      lcd.setCursor(0, 1);
      lcd.print("Fechando cofre");
    }

    tone(buzzer, 1500);
    delay(150);
    noTone(buzzer);

    delay(1000);

    if (cofreAberto) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Cofre aberto!");
    } else {
      mostrarTelaInicial();
    }

  } else {

    tentativasErradas++;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SENHA INVALIDA");

    tone(buzzer, 400);
    delay(500);
    noTone(buzzer);

    delay(1500);

    if (tentativasErradas >= 3) {
      dispararAlarme();
    } else {
      mostrarTelaInicial();
    }
  }
}

// VERIFICAR LASER E ULTRASSONICO
void verificarSensores() {

  if (cofreAberto) {
    presencaDetectada = false;
    return;
  }

  // LEITURA ANALOGICA DO LDR
  leituraLDR = analogRead(sensorLDR);

  // CONVERTER LEITURA EM TENSAO
  tensaoLDR = leituraLDR * 5.0 / 1023.0;

  // MOSTRAR LEITURA E TENSAO NO SERIAL
  if (millis() - tempoLeituraLDR >= 500) {
    tempoLeituraLDR = millis();

    Serial.print("Leitura LDR: ");
    Serial.print(leituraLDR);

    Serial.print(" | Tensao: ");
    Serial.print(tensaoLDR);
    Serial.print("V");

    if (leituraLDR < limite) {
      Serial.println(" | Laser interrompido");
    } else {
      Serial.println(" | Laser detectado");
    }
  }

  // ABAIXO DE 600 = LASER OBSTRUIDO
  // 600 OU MAIS = LASER DETECTADO
  bool laserInterrompido = leituraLDR < limite;

  // ULTRASSONICO CONTROLA SOMENTE O LED VERDE
  float distancia = medirDistancia();

  presencaDetectada =
    distancia > 0 && distancia <= distanciaLimite;

  // ULTRASSONICO NAO DISPARA O ALARME
  if (sensorAguardandoLiberacao) {

    if (!laserInterrompido) {
      sensorAguardandoLiberacao = false;
    } else {
      return;
    }
  }

  // SOMENTE O LDR DISPARA O ALARME
  if (laserInterrompido && !invasao && !alarmeAtivo) {
    dispararAlarme();
  }
}

// BOTAO PARA DESLIGAR ALARME
void verificarBotao() {

  if (digitalRead(botaoAlarme) == LOW) {
    delay(30);

    if (digitalRead(botaoAlarme) == LOW) {

      alarmeAtivo = false;

      noTone(buzzer);

      servo.write(servoFechado);
      cofreAberto = false;

      digitalWrite(laser, HIGH);

      digitalWrite(led, LOW);
      digitalWrite(ledVermelho, LOW);

      estadoLedVermelho = false;

      invasao = false;
      tentativasErradas = 0;

      // VERIFICAR SOMENTE O LDR
      bool laserAtivo =
        analogRead(sensorLDR) < limite;

      sensorAguardandoLiberacao = laserAtivo;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ALARME DESLIGADO");

      lcd.setCursor(0, 1);
      lcd.print("Cofre fechado");

      while (digitalRead(botaoAlarme) == LOW) {
        delay(10);
      }

      delay(1000);

      mostrarTelaInicial();
    }
  }
}

// LER TECLADO
char lerTeclado() {

  for (int l = 0; l < linhas; l++) {

    digitalWrite(pinosLinhas[l], LOW);

    for (int c = 0; c < colunas; c++) {

      if (digitalRead(pinosColunas[c]) == LOW) {

        char tecla = teclas[l][c];

        digitalWrite(pinosLinhas[l], HIGH);

        return tecla;
      }
    }

    digitalWrite(pinosLinhas[l], HIGH);
  }

  return '\0';
}

// CONFIGURACOES INICIAIS
void setup() {

  Wire.begin();

  Serial.begin(9600);

  pinMode(buzzer, OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(ledVermelho, OUTPUT);

  pinMode(sensorLDR, INPUT);
  pinMode(botaoAlarme, INPUT_PULLUP);

  pinMode(laser, OUTPUT);
  digitalWrite(laser, HIGH);

  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);

  digitalWrite(trig, LOW);

  servo.attach(motor);
  servo.write(servoFechado);

  digitalWrite(led, LOW);
  digitalWrite(ledVermelho, LOW);

  noTone(buzzer);

  for (int i = 0; i < linhas; i++) {
    pinMode(pinosLinhas[i], OUTPUT);
    digitalWrite(pinosLinhas[i], HIGH);
  }

  for (int i = 0; i < colunas; i++) {
    pinMode(pinosColunas[i], INPUT_PULLUP);
  }

  int statusLCD = lcd.begin(16, 2);

  if (statusLCD != 0) {
    while (true) {
    }
  }

  lcd.backlight();
  lcd.clear();

  mostrarTelaInicial();
}

// PROGRAMA PRINCIPAL
void loop() {

  // GARANTIR ESTADO DO LASER
  if (cofreAberto) {
    digitalWrite(laser, LOW);
  } else {
    digitalWrite(laser, HIGH);
  }

  verificarSensores();
  verificarBotao();

  // GARANTIR ESTADO DO LASER NOVAMENTE
  if (cofreAberto) {
    digitalWrite(laser, LOW);
  } else {
    digitalWrite(laser, HIGH);
  }

  controlarLeds();

  if (alarmeAtivo) {
    tocarAlarme();
    return;
  }

  if (invasao) return;

  char tecla = lerTeclado();

  if (tecla != '\0' && tecla != 'X') {

    bipTecla();

    if (tecla >= '0' && tecla <= '9') {

      if (posicao < 4) {

        senha[posicao] = tecla;

        lcd.setCursor(posicao, 1);
        lcd.print(tecla);

        posicao++;

        if (posicao == 4) {
          verificarSenha();
        }
      }
    }

    if (tecla == '*') {
      apagarSenha();
    }

    if (tecla == '#') {
      // Reservado
    }

    delay(150);

    while (lerTeclado() != '\0') {
      delay(20);
    }
  }
}
