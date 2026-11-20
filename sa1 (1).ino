#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <Servo.h>
#include <string.h>

// COMPONENTES
const int buzzer = 12;
const int motor = 9;
const int led = 8;
const int sensorLDR = A0;
const int botaoAlarme = 13;
const int sensorPIR = A1;
const int laser = 10;

// SERVO
Servo servo;
const int servoFechado = 0;
const int servoAberto = 90;

// SENSOR LDR
const int limite = 500;
bool sensorAguardandoLiberacao = false;

// PIR
unsigned long inicioSistema;
const unsigned long tempoEstabilizacao = 30000;

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
byte pinosColunas[4] = {6, 7, A2, A3};

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

// DISPARAR ALARME
void dispararAlarme() {
  if (alarmeAtivo) return;

  alarmeAtivo = true;
  invasao = true;

  servo.write(servoFechado);
  cofreAberto = false;

  digitalWrite(led, HIGH);

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

    servo.write(servoAberto);
    cofreAberto = true;

    digitalWrite(led, LOW);

    tone(buzzer, 1500);
    delay(150);
    noTone(buzzer);

    delay(1000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Cofre aberto!");

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

// VERIFICAR LASER E PIR
void verificarSensores() {
  int valorLDR = analogRead(sensorLDR);

  bool laserInterrompido = valorLDR < limite;

  bool pirEstabilizado =
    millis() - inicioSistema >= tempoEstabilizacao;

  bool movimentoDetectado =
    pirEstabilizado && digitalRead(sensorPIR) == HIGH;

  bool sensorAtivo =
    laserInterrompido || movimentoDetectado;

  Serial.print("LDR: ");
  Serial.print(valorLDR);
  Serial.print(" | PIR: ");
  Serial.println(digitalRead(sensorPIR));

  if (sensorAguardandoLiberacao) {
    if (!sensorAtivo) {
      sensorAguardandoLiberacao = false;
    } else {
      return;
    }
  }

  if (sensorAtivo && !invasao && !alarmeAtivo) {
    dispararAlarme();
  }
}

// BOTÃO PARA DESLIGAR ALARME
void verificarBotao() {
  if (digitalRead(botaoAlarme) == LOW) {
    delay(30);

    if (digitalRead(botaoAlarme) == LOW) {
      alarmeAtivo = false;

      noTone(buzzer);

      servo.write(servoFechado);
      cofreAberto = false;

      digitalWrite(led, LOW);

      invasao = false;
      tentativasErradas = 0;

      bool laserAtivo = analogRead(sensorLDR) < limite;

      bool pirAtivo =
        millis() - inicioSistema >= tempoEstabilizacao &&
        digitalRead(sensorPIR) == HIGH;

      sensorAguardandoLiberacao = laserAtivo || pirAtivo;

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

// CONFIGURAÇÕES INICIAIS
void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(buzzer, OUTPUT);
  pinMode(led, OUTPUT);

  pinMode(sensorLDR, INPUT);
  pinMode(botaoAlarme, INPUT_PULLUP);
  pinMode(sensorPIR, INPUT);

  pinMode(laser, OUTPUT);
  digitalWrite(laser, HIGH);

  servo.attach(motor);
  servo.write(servoFechado);

  digitalWrite(led, LOW);
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
    Serial.print("Erro ao iniciar LCD: ");
    Serial.println(statusLCD);

    while (true) {
    }
  }

  lcd.backlight();
  lcd.clear();

  inicioSistema = millis();

  mostrarTelaInicial();
}

// PROGRAMA PRINCIPAL
void loop() {
  verificarSensores();
  verificarBotao();

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