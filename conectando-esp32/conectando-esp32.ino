#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "DHT.h"  // Biblioteca do Sensor DHT

// --- 1. DEFINIÇÕES DE HARDWARE ---
#define DHTPIN 4
#define DHTTYPE DHT11
#define LED_PIN 2

DHT dht(DHTPIN, DHTTYPE); // Cria o objeto DHT

// --- 2. CONFIGURAÇÕES DE REDE E MQTT ---
const char* ssid = "Gio";
const char* password = "610v4nn4";

// Credenciais do EMQX Cloud
const char* mqtt_server = "k7e6191e.ala.us-east-1.emqxsl.com";
const int mqtt_port = 8883;
const char* mqtt_user = "Giovanna";
const char* mqtt_password = "610v4nn4";

// Tópicos de Publicação e Assinatura
const char* TOPIC_TEMP = "lab/sala/temperatura";
const char* TOPIC_HUMIDITY = "lab/sala/umidade";
const char* TOPIC_LED = "esp32/led";

// --- 3. VARIÁVEIS DE CONTROLE ---
WiFiClientSecure espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
const long interval = 10000; // Publica dados a cada 10 segundos

// Certificado CA do Broker
const char* ca_cert = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
"MrY=\n"
"-----END CERTIFICATE-----";

// --- DECLARAÇÃO DE FUNÇÕES ---
void setup_wifi();
void reconnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);

// --- 4. FUNÇÃO DE SETUP ---
void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  dht.begin();          // Inicializa o sensor DHT
  setup_wifi();         // Conecta ao Wi-Fi

  espClient.setCACert(ca_cert); // Configura o certificado SSL
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
}

// --- 5. CONEXÃO WI-FI ---
void setup_wifi() {
  delay(10);
  Serial.print("Conectando a ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("Conectado ao Wi-Fi!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// --- 6. FUNÇÃO DE CALLBACK (RECEBIMENTO DE DADOS) ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == TOPIC_LED) {
    if (message == "ON") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED ligado");
    } else if (message == "OFF") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED desligado");
    }
  }
}

// --- 7. FUNÇÃO DE RECONEXÃO MQTT ---
void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexao MQTT...");
    if (client.connect("ESP32_DHT_Client", mqtt_user, mqtt_password)) {
      Serial.println("Conectado!");
      client.subscribe("esp32/led");
    } else {
      Serial.print("Falha! rc=");
      Serial.print(client.state());
      Serial.println(" Tentando em 5 segundos...");
      delay(5000);
    }
  }
}

// --- 8. LOOP PRINCIPAL ---
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantém a conexão MQTT ativa

  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;

    // Leitura do sensor DHT11
    float umidade = dht.readHumidity();
    float temperatura = dht.readTemperature();

    if (isnan(umidade) || isnan(temperatura)) {
      Serial.println("Falha na leitura do sensor DHT!");
      return;
    }

    // --- Publicação da TEMPERATURA ---
    char tempPayload[10];
    dtostrf(temperatura, 4, 2, tempPayload);
    client.publish(TOPIC_TEMP, tempPayload);
    Serial.print("Publicado [");
    Serial.print(TOPIC_TEMP);
    Serial.print("]: ");
    Serial.println(tempPayload);

    // --- Publicação da UMIDADE ---
    char humiPayload[10];
    dtostrf(umidade, 4, 2, humiPayload);
    client.publish(TOPIC_HUMIDITY, humiPayload);
    Serial.print("Publicado [");
    Serial.print(TOPIC_HUMIDITY);
    Serial.print("]: ");
    Serial.println(humiPayload);
  }
}