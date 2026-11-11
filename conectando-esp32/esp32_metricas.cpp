#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <time.h>

// 1. CONFIGURACAO DO WI-FI

const char* WIFI_SSID = "Lucas";
const char* WIFI_PASSWORD = "150471Mfm";

// 2. CONFIGURACAO DO EMQX CLOUD

const char* MQTT_SERVER =
    "r156dad5.ala.eu-central-1.emqxsl.com";

const uint16_t MQTT_PORT = 8883;

const char* MQTT_USER = "Lucas";
const char* MQTT_PASSWORD = "131205Lm_";

// 3. TOPICOS MQTT

const char* TOPICO_UMIDADE =
    "lab/sala/umidade_solo";

const char* TOPICO_LED =
    "esp32/led";

// 4. PINOS

const int PINO_SENSOR = 34;
const int PINO_LED = 2;

// 5. CALIBRACAO

// Estes valores serao medidos e salvos
// pelo proprio usuario via Monitor Serial.

int valorSeco = 0;
int valorMolhado = 0;

bool secoCalibrado = false;
bool molhadoCalibrado = false;

const int DIFERENCA_MINIMA_ADC = 60;

// 6. FILTRAGEM

const int NUM_LEITURAS = 20;
const int DESCARTAR_EXTREMOS = 4;

const float ALPHA = 0.25f;

float adcFiltrado = 0;
bool primeiraLeitura = true;

// 7. INTERVALOS

const unsigned long INTERVALO_PUBLICACAO = 5000;
const unsigned long INTERVALO_RECONEXAO = 5000;
const unsigned long TIMEOUT_WIFI = 20000;

unsigned long ultimaLeitura = 0;
unsigned long ultimaTentativaWiFi = 0;
unsigned long ultimaTentativaMQTT = 0;
unsigned long inicioConexaoWiFi = 0;
unsigned long ultimoAvisoWiFi = 0;

bool tentandoWiFi = false;
bool horarioConfigurado = false;

// 8. CERTIFICADO CA ORIGINAL

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
"-----END CERTIFICATE-----\n";

// 9. CLIENTES

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Preferences preferencias;

// 10. VERIFICACAO DE CALIBRACAO

bool calibracaoValida() {

  return secoCalibrado &&
         molhadoCalibrado &&
         abs(valorMolhado - valorSeco) >=
             DIFERENCA_MINIMA_ADC;
}

// 11. CARREGAR CALIBRACAO SALVA

void carregarCalibracao() {

  preferencias.begin("umidade", false);

  valorSeco = preferencias.getInt("seco", 0);
  valorMolhado = preferencias.getInt("molhado", 0);

  secoCalibrado =
      preferencias.getBool("temSeco", false);

  molhadoCalibrado =
      preferencias.getBool("temMolhado", false);

  if (calibracaoValida()) {

    Serial.println("Calibracao carregada!");

    Serial.print("ADC seco: ");
    Serial.println(valorSeco);

    Serial.print("ADC molhado: ");
    Serial.println(valorMolhado);

  } else {

    Serial.println("Sensor ainda nao calibrado.");
    Serial.println("Envie S para calibrar seco.");
    Serial.println("Envie M para calibrar molhado.");
  }
}

// 12. LEITURA ANALOGICA COM MEDIA

int lerADCMedio() {

  int leituras[NUM_LEITURAS];

  // Primeira conversao descartada
  analogRead(PINO_SENSOR);
  delay(5);

  for (int i = 0; i < NUM_LEITURAS; i++) {

    leituras[i] = analogRead(PINO_SENSOR);
    delay(3);
  }

  // Ordenar as leituras
  for (int i = 0; i < NUM_LEITURAS - 1; i++) {

    for (int j = i + 1; j < NUM_LEITURAS; j++) {

      if (leituras[j] < leituras[i]) {

        int temporario = leituras[i];
        leituras[i] = leituras[j];
        leituras[j] = temporario;
      }
    }
  }

  // Descartar os extremos
  long soma = 0;
  int quantidade = 0;

  for (int i = DESCARTAR_EXTREMOS;
       i < NUM_LEITURAS - DESCARTAR_EXTREMOS;
       i++) {

    soma += leituras[i];
    quantidade++;
  }

  return (int)(soma / quantidade);
}

// 13. CALIBRACAO PELO MONITOR SERIAL

void verificarComandosSerial() {

  while (Serial.available() > 0) {

    char comando = Serial.read();

    if (comando == 'S' || comando == 's') {

      int leitura = lerADCMedio();

      valorSeco = leitura;
      secoCalibrado = true;

      preferencias.putInt("seco", valorSeco);
      preferencias.putBool("temSeco", true);

      primeiraLeitura = true;

      Serial.println();
      Serial.println("CALIBRACAO SECA REGISTRADA!");

      Serial.print("ADC seco: ");
      Serial.println(valorSeco);
    }

    if (comando == 'M' || comando == 'm') {

      int leitura = lerADCMedio();

      valorMolhado = leitura;
      molhadoCalibrado = true;

      preferencias.putInt(
          "molhado",
          valorMolhado
      );

      preferencias.putBool(
          "temMolhado",
          true
      );

      primeiraLeitura = true;

      Serial.println();
      Serial.println("CALIBRACAO MOLHADA REGISTRADA!");

      Serial.print("ADC molhado: ");
      Serial.println(valorMolhado);
    }

    if (comando == 'S' || comando == 's' ||
        comando == 'M' || comando == 'm') {

      if (calibracaoValida()) {

        Serial.println("CALIBRACAO VALIDA!");
        Serial.println("Medicao habilitada.");

      } else {

        Serial.println(
            "Calibracao incompleta ou intervalo "
            "ADC muito pequeno."
        );
      }
    }
  }
}

// 14. LEITURA DE UMIDADE

int lerUmidade() {

  int adcMedio = lerADCMedio();

  if (primeiraLeitura) {

    adcFiltrado = adcMedio;
    primeiraLeitura = false;

  } else {

    adcFiltrado =
        ALPHA * adcMedio +
        (1.0f - ALPHA) * adcFiltrado;
  }

  Serial.println();
  Serial.println("===== SENSOR RESISTIVO =====");

  Serial.print("ADC medio: ");
  Serial.println(adcMedio);

  Serial.print("ADC filtrado: ");
  Serial.println(adcFiltrado, 1);

  if (!calibracaoValida()) {

    Serial.println("Calibracao pendente.");
    Serial.println("Envie S (seco) e M (molhado).");
    Serial.println("============================");

    return -1;
  }

  // Conversao proporcional entre seco e molhado
  // Funciona mesmo se o ADC subir com a umidade

  float umidade =
      (adcFiltrado - valorSeco) *
      100.0f /
      (valorMolhado - valorSeco);

  umidade = constrain(
      umidade,
      0.0f,
      100.0f
  );

  int porcentagem = (int)(umidade + 0.5f);

  Serial.print("Umidade do solo: ");
  Serial.print(porcentagem);
  Serial.println("%");

  Serial.println("============================");

  return porcentagem;
}

// 15. EVENTOS WI-FI

void eventoWiFi(
    WiFiEvent_t event,
    WiFiEventInfo_t info) {

  switch (event) {

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("Conectado ao hotspot!");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WI-FI CONECTADO!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.print("Wi-Fi desconectado. Motivo: ");
      Serial.println(
          info.wifi_sta_disconnected.reason
      );
      break;

    default:
      break;
  }
}

// 16. CONEXAO WI-FI

void conectarWiFi() {

  if (WiFi.status() == WL_CONNECTED ||
      tentandoWiFi) {
    return;
  }

  Serial.println();
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(WIFI_SSID);

  tentandoWiFi = true;

  inicioConexaoWiFi = millis();
  ultimaTentativaWiFi = millis();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// 17. GERENCIAMENTO WI-FI

void verificarWiFi() {

  unsigned long agora = millis();

  if (WiFi.status() == WL_CONNECTED) {

    tentandoWiFi = false;
    return;
  }

  if (tentandoWiFi) {

    if (agora - ultimoAvisoWiFi >= 5000) {

      ultimoAvisoWiFi = agora;

      Serial.print("Aguardando Wi-Fi. Estado: ");
      Serial.println(WiFi.status());
    }

    if (agora - inicioConexaoWiFi >= TIMEOUT_WIFI) {

      Serial.println("TIMEOUT WI-FI!");

      WiFi.disconnect(false, false);

      tentandoWiFi = false;
      ultimaTentativaWiFi = millis();
    }

    return;
  }

  if (agora - ultimaTentativaWiFi >=
      INTERVALO_RECONEXAO) {

    conectarWiFi();
  }
}

// 18. SINCRONIZACAO DE HORARIO

bool sincronizarHorario() {

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (!horarioConfigurado) {

    Serial.println("Sincronizando horario NTP...");

    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.google.com"
    );

    horarioConfigurado = true;
  }

  return time(nullptr) > 1700000000;
}

// 19. CALLBACK MQTT

void callback(
    char* topic,
    byte* payload,
    unsigned int length) {

  String mensagem = "";

  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  mensagem.trim();
  mensagem.toUpperCase();

  Serial.print("Mensagem recebida: ");
  Serial.println(mensagem);

  if (String(topic) == TOPICO_LED) {

    if (mensagem == "ON") {

      digitalWrite(PINO_LED, HIGH);
      Serial.println("LED LIGADO");

    } else if (mensagem == "OFF") {

      digitalWrite(PINO_LED, LOW);
      Serial.println("LED DESLIGADO");
    }
  }
}

// 20. CONEXAO MQTT

void conectarMQTT() {

  if (WiFi.status() != WL_CONNECTED ||
      mqttClient.connected()) {
    return;
  }

  if (!sincronizarHorario()) {

    Serial.println("Aguardando horario NTP...");
    return;
  }

  String clientId = "ESP32-SOLO-";

  clientId += String(
      (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF),
      HEX
  );

  clientId += "-";
  clientId += String(esp_random(), HEX);

  Serial.println();
  Serial.println("Tentando conexao MQTT com TLS...");

  if (mqttClient.connect(
          clientId.c_str(),
          MQTT_USER,
          MQTT_PASSWORD)) {

    Serial.println("MQTT CONECTADO!");

    if (mqttClient.subscribe(TOPICO_LED)) {

      Serial.print("Inscrito em: ");
      Serial.println(TOPICO_LED);

    } else {

      Serial.println("Falha ao assinar topico LED");
    }

  } else {

    Serial.print("Falha MQTT! rc=");
    Serial.println(mqttClient.state());

    char erroTLS[256] = {0};

    int codigoTLS = espClient.lastError(
        erroTLS,
        sizeof(erroTLS)
    );

    Serial.print("Codigo TLS: ");
    Serial.println(codigoTLS);

    Serial.print("Descricao TLS: ");
    Serial.println(erroTLS);
  }
}

// 21. PUBLICACAO MQTT

void publicarUmidade(int umidade) {

  if (umidade < 0) {
    Serial.println("Aguardando calibracao.");
    return;
  }

  if (!mqttClient.connected()) {

    Serial.println(
        "MQTT desconectado. Leitura local."
    );

    return;
  }

  String mensagem = String(umidade);

  bool publicado = mqttClient.publish(
      TOPICO_UMIDADE,
      mensagem.c_str()
  );

  if (publicado) {

    Serial.print("Publicado [");
    Serial.print(TOPICO_UMIDADE);
    Serial.print("]: ");
    Serial.println(mensagem);

  } else {

    Serial.println("Falha ao publicar MQTT");
  }
}

// 22. SETUP

void setup() {

  Serial.begin(9600);
  delay(2000);

  Serial.println();
  Serial.println("INICIANDO SISTEMA ESP32");

  pinMode(PINO_LED, OUTPUT);
  digitalWrite(PINO_LED, LOW);

  pinMode(PINO_SENSOR, INPUT);

  analogReadResolution(12);

  analogSetPinAttenuation(
      PINO_SENSOR,
      ADC_11db
  );

  carregarCalibracao();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);

  WiFi.onEvent(eventoWiFi);

  espClient.setCACert(ca_cert);

  mqttClient.setServer(
      MQTT_SERVER,
      MQTT_PORT
  );

  mqttClient.setCallback(callback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(5);

  conectarWiFi();

  ultimaLeitura = millis();

  Serial.println("Sistema configurado!");
  Serial.println("Comandos: S=seco, M=molhado");
}

// 23. LOOP PRINCIPAL

void loop() {

  verificarComandosSerial();

  unsigned long agora = millis();

  if (agora - ultimaLeitura >=
      INTERVALO_PUBLICACAO) {

    ultimaLeitura = agora;

    int umidade = lerUmidade();

    publicarUmidade(umidade);
  }

  verificarWiFi();

  if (WiFi.status() != WL_CONNECTED) {

    delay(10);
    return;
  }

  agora = millis();

  if (!mqttClient.connected()) {

    if (agora - ultimaTentativaMQTT >=
        INTERVALO_RECONEXAO) {

      ultimaTentativaMQTT = agora;

      conectarMQTT();
    }

  } else {

    mqttClient.loop();
  }

  delay(10);
}
