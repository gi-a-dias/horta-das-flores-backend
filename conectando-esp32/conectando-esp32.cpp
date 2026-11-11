
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

// 1. CONFIGURACAO DO WI-FI

const char* WIFI_SSID = "iPhone de Lucas";
const char* WIFI_PASSWORD = "masteguim1312";

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

// 4. PINOS DO ESP32

// A0 do sensor resistivo no GPIO 34
const int PINO_SENSOR = 34;
const int PINO_LED = 2;

// 5. CALIBRACAO DO SENSOR

const int VALOR_SECO = 3200;
const int VALOR_MOLHADO = 1500;

// 6. INTERVALOS

const unsigned long INTERVALO_PUBLICACAO = 10000;
const unsigned long INTERVALO_RECONEXAO = 5000;
const unsigned long TIMEOUT_WIFI = 20000;

// 7. CERTIFICADO CA ORIGINAL

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

// 8. VARIAVEIS DE CONTROLE

unsigned long ultimaLeitura = 0;
unsigned long ultimaTentativaWiFi = 0;
unsigned long ultimaTentativaMQTT = 0;
unsigned long inicioConexaoWiFi = 0;
unsigned long ultimoAvisoWiFi = 0;

bool tentandoWiFi = false;
bool horarioConfigurado = false;

int ultimaUmidade = 0;

// 9. CLIENTES

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// 10. LEITURA DO SENSOR RESISTIVO

int lerUmidade() {

  int valorADC = analogRead(PINO_SENSOR);

  long umidade = map(
      valorADC,
      VALOR_SECO,
      VALOR_MOLHADO,
      0,
      100
  );

  umidade = constrain(umidade, 0, 100);

  Serial.println();
  Serial.println("===== SENSOR RESISTIVO =====");

  Serial.print("GPIO: ");
  Serial.println(PINO_SENSOR);

  Serial.print("Valor analogico: ");
  Serial.println(valorADC);

  Serial.print("Umidade do solo: ");
  Serial.print(umidade);
  Serial.println("%");

  Serial.println("============================");

  return (int)umidade;
}

// 11. EVENTOS DO WI-FI

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

// 12. CONEXAO WI-FI

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

// 13. GERENCIAMENTO DO WI-FI

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

// 14. SINCRONIZACAO NTP

bool sincronizarHorario() {

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (!horarioConfigurado) {

    Serial.println("Iniciando sincronizacao NTP...");

    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.google.com"
    );

    horarioConfigurado = true;
  }

  time_t agora = time(nullptr);

  if (agora > 1700000000) {
    return true;
  }

  return false;
}

// 15. RECEBIMENTO MQTT

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

    } else {

      Serial.println("Comando invalido");
    }
  }
}

// 16. CONEXAO MQTT

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

  Serial.print("Broker: ");
  Serial.println(MQTT_SERVER);

  Serial.print("Porta: ");
  Serial.println(MQTT_PORT);

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

    int estado = mqttClient.state();

    Serial.print("Falha MQTT! rc=");
    Serial.println(estado);

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

// 17. PUBLICACAO DA UMIDADE

void publicarUmidade(int umidade) {

  if (!mqttClient.connected()) {

    Serial.println(
        "MQTT desconectado. Leitura apenas local."
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

// 18. SETUP

void setup() {

  // VELOCIDADE SERIAL ALTERADA
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

  // Primeira leitura sem depender da rede
  ultimaUmidade = lerUmidade();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);

  WiFi.onEvent(eventoWiFi);

  // TLS com certificado original
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
}

// 19. LOOP PRINCIPAL

void loop() {

  unsigned long agora = millis();

  // Leitura independente do Wi-Fi e MQTT
  if (agora - ultimaLeitura >=
      INTERVALO_PUBLICACAO) {

    ultimaLeitura = agora;

    ultimaUmidade = lerUmidade();

    publicarUmidade(ultimaUmidade);
  }

  // Verificacao Wi-Fi
  verificarWiFi();

  if (WiFi.status() != WL_CONNECTED) {

    delay(10);
    return;
  }

  // Verificacao MQTT
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
