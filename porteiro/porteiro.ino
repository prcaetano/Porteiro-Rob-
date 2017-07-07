/*
Código Porteiro eletrônico controlado por chatbot

Baseado em:
* bot.ino (https://github.com/lfgomez/F541/tree/master/bot) por Luis Fernando Gomez Gonzalez (luis.gonzalez@inmetrics.com.br)
* exemplos WifiManager (https://github.com/tzapu/WiFiManager)
* exemplos ESP8266-TelegramBot (https://github.com/Gianbacchio/ESP8266-TelegramBot)
*/

#include <FS.h>
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <ArduinoJson.h>

#include "ESP8266TelegramBOT.h"  // versão modificada
#include <Ticker.h>

// pinos utilizados
#define RELE 4  // relé em D2 = GPIO 4
#define LED_TRANCADO 14 // led vermelho em D5 = GPIO 14
#define LED_DESTRANCADO 12 // led verde em D6 = GPIO 12
#define LED_ABERTO 13 // led verde em D7 = GPIO 13
#define BOTAO 15 // botao em D8 = GPIO 15

#define TEMPO_ABERTURA 5000  // tempo que a porta permanece aberta

bool porta_aberta = false;  // flag registrando se a porta está aberta
long botao_lasttime;  // instante em que o botão de abertura foi acionado por último
int botao_delay = 3000;  // delay após o botão ser ativado no qual o bot o ignora

// Variáveis de configuração do BOT Telegram
char BOTtoken[47];
char BOTname[51];
char BOTusername[51];
char BOTmaster[51];

// flag notificando se configurações devem ser salvas
bool shouldSaveConfig = false;

// SSID e senha do AP
char APname[15] = "PorteiroConfig";
char APpass[16] = "";

// variáveis de controle do bot
int Bot_mtbs = 500; //tempo médio entre escaneamentos de mensagens
long Bot_lasttime; //última vez que o escaneamento foi feito

// Bot telegram
TelegramBOT bot(BOTtoken, BOTname, BOTusername);

// Salva configurações quando parâmetros personalizados foram setados
// e conexão ocorreu com sucesso
void saveConfigCallback(){
  Serial.println("Salvar a configuração.");
  shouldSaveConfig = true;
}

// Monta o sistema de arquivo e le o arquivo config.json
void spiffsMount(char BOTtoken[], char BOTusername[], char BOTname[], char BOTmaster[]) {
  // para debug
  if (SPIFFS.begin()) {
    Serial.println("Sistema de Arquivos Montado");
    if (SPIFFS.exists("/config.json")) {
      Serial.println("Arquivo já existente, lendo..");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Arquivo aberto com sucesso");
        size_t size = configFile.size();
        // Criando um Buffer para alocar os dados do arquivo.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nJson Traduzido:");

          if (json.containsKey("BOTtoken")) {
            strcpy(BOTtoken, json["BOTtoken"]);
          }
          else {
            strcpy(BOTtoken, "");
          }
          if (json.containsKey("BOTname")) {
            strcpy(BOTname, json["BOTname"]);
          }
          else {
            strcpy(BOTname, "");
          }
          if (json.containsKey("BOTusername")) {
            strcpy(BOTusername, json["BOTusername"]);
          }
          else {
            strcpy(BOTusername, "");
          }
          if (json.containsKey("BOTmaster")) {
            strcpy(BOTmaster, json["BOTmaster"]);
          }
          else {
            strcpy(BOTmaster, "");
          }
        }
          else {
          Serial.println("Falha em ler o Arquivo");
        }
      }
    }
  }
  else {
    Serial.println("Falha ao montar o sistema de arquivos");
  }
}

// Salva configurações em formato json no sistema de arquivos
void saveConfig(char BOTtoken[], char BOTusername[], char BOTname[], char BOTmaster[]) {
  Serial.println("Salvando configurações...");

  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["BOTtoken"] = BOTtoken;
  json["BOTname"] = BOTname;
  json["BOTusername"] = BOTusername;
  json["BOTmaster"] = BOTmaster;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
   Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
}

void setup(){
  Serial.begin(115200);
  Serial.println("Oi :)");

  Serial.println("Montando sistema de arquivos...");
  spiffsMount(BOTtoken, BOTname, BOTusername, BOTmaster);

  // Inicialização do WiFiManager
  WiFiManager wifiManager;

  // Criando parâmetros personalizados de configuração
  // para o Bot Telegram
  WiFiManagerParameter BOTtoken_config("token", "Token do Bot Telegram", "", 46);
  WiFiManagerParameter BOTname_config("name", "Nome do Bot Telegram", "", 50);
  WiFiManagerParameter BOTusername_config("username", "Username do Bot Telegram", "", 50);
  WiFiManagerParameter BOTmaster_config("botmaster", "Username telegram do dono do Bot", "", 50);

  // Registrando callback de notificação de salvamento de configurações
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Adicionando parâmetros de configuração
  // do bot Telegram
  wifiManager.addParameter(&BOTtoken_config);
  wifiManager.addParameter(&BOTname_config);
  wifiManager.addParameter(&BOTusername_config);
  wifiManager.addParameter(&BOTmaster_config);

  // Carrega ssid e senha configurados e tenta conectar
  // Em caso de falha, cria AP e espera configuração
  if(!wifiManager.autoConnect(APname, APpass)) {
    Serial.println("Timeout.");
    Serial.println("Não consegui conexão WiFi.");
    delay(3000);
    // em caso de timeout, reset
    ESP.reset();
    delay(5000);
  }

  Serial.println("Conectado!");
  // atualiza configuração do bot - função personalizada
  bot.configuraBOT(BOTtoken, BOTname, BOTusername);

  // devo salvar as configurações?
  if(shouldSaveConfig) {
    // lendo valores atualizados dos parâmetros do bot
    strcpy(BOTtoken, BOTtoken_config.getValue());
    strcpy(BOTname, BOTname_config.getValue());
    strcpy(BOTusername, BOTusername_config.getValue());
    strcpy(BOTmaster, BOTmaster_config.getValue());
    // salvando configurações
    saveConfig(BOTtoken, BOTname, BOTusername, BOTmaster);
    delay(1000);
    ESP.reset();
  }

  Serial.println("Ip local:");
  Serial.println(WiFi.localIP());

  pinMode(RELE, OUTPUT);
  pinMode(LED_DESTRANCADO, OUTPUT);
  pinMode(LED_TRANCADO, OUTPUT);
  pinMode(LED_ABERTO, OUTPUT);
  pinMode(BOTAO, INPUT);
}

// Processa mensagens recebidas por telegram
void Bot_ExecMessages() {
  for (int i = 1; i < bot.message[0][0].toInt() + 1; i++) {
    bot.message[i][5]=bot.message[i][5].substring(0,bot.message[i][5].length());
    Serial.print("Mensagem: ");
    Serial.println(bot.message[i][5]);

  if (bot.message[i][5].length() > 0) {

    if (bot.message[i][5] == "/start") {
      String wellcome = "Olá! Eu sou o Porteiro-Robô! O que deseja fazer?";
      String wellcome1 = "/abra: abre a porta";
      String wellcome2 = "/destranca: destranca a porta";
      String wellcome3 = "/tranca: tranca a porta";
      String wellcome4 = "/status: status da porta";
      bot.sendMessage(bot.message[i][4], wellcome, "");
      bot.sendMessage(bot.message[i][4], wellcome1, "");
      bot.sendMessage(bot.message[i][4], wellcome2, "");
      bot.sendMessage(bot.message[i][4], wellcome3, "");
      bot.sendMessage(bot.message[i][4], wellcome4, "");
     }

     if (bot.message[i][5] == "/abra") {
       String message = "clift cloft Still, a porta se abriu!";
       bot.sendMessage(bot.message[i][4], message, "");
       abre_porta();
     }
     if (bot.message[i][5] == "/tranca") {
      String message = "A porta foi trancada.";
      bot.sendMessage(bot.message[i][4], message, "");
      tranca_porta();
     }
     if (bot.message[i][5] == "/destranca") {
      String message = "A porta foi destrancada.";
      bot.sendMessage(bot.message[i][4], message, "");
      destranca_porta();
     }
     if (bot.message[i][5] == "/status") {
      if (porta_aberta) {
        String message = "A porta está aberta.";
        bot.sendMessage(bot.message[i][4], message, "");
      }
      else {
        String message = "A porta está fechada.";
        bot.sendMessage(bot.message[i][4], message, "");
      }
     }
    }
  }
  bot.message[0][0] = "";   // respondemos todas as mensagens e limpamos o buffer
}

// função executada quando bot telegram recebe comando /abra
// ou o botão é pressionado e a porta está destrancada
void abre_porta(){
  digitalWrite(RELE, HIGH);
  digitalWrite(LED_ABERTO, HIGH);
  Serial.println("Relé ligou.");
  delay(TEMPO_ABERTURA);
  digitalWrite(RELE, LOW);
  digitalWrite(LED_ABERTO, LOW);
  Serial.println("Relé desligou.");
}

// função executada quando o bot recebe o comando /tranca
void tranca_porta(){
  porta_aberta = false;
}

// função executada quando o bot recebe o comando /destranca
void destranca_porta(){
  porta_aberta = true;
}

void loop(){
  if (millis() > Bot_lasttime + Bot_mtbs){  // recebe e processa mensagens do bot a cada Bot_mtbs segundos
    bot.getUpdates(bot.message[0][1]);
    Bot_ExecMessages();
    Bot_lasttime = millis();
  }
  if (porta_aberta & digitalRead(BOTAO) == HIGH & (millis() > botao_lasttime + botao_delay)) {
    digitalWrite(LED_DESTRANCADO, HIGH);  // abre a porta se o botão for apertado, a porta estiver destrancada
    digitalWrite(LED_TRANCADO, LOW);      // e passou mais que botao_delay desde a última vez que a porta foi aberta pelo botão
    botao_lasttime = millis();
    abre_porta();
  }
  else if (!porta_aberta) {
    digitalWrite(LED_TRANCADO, HIGH);
    digitalWrite(LED_DESTRANCADO, LOW);
  }
  else {
    digitalWrite(LED_DESTRANCADO, HIGH);
    digitalWrite(LED_TRANCADO, LOW);
  }
}
