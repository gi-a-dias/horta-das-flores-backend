#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// true  = TESTE: não valida o certificado (só para diagnosticar)
// false = uso normal: valida com o certificado CA abaixo
#define TESTE_SEM_CERTIFICADO false

// --- 1. DEFINIÇÕES DE HARDWARE ---
#define PIN_ANALOGICO 36   // VP (GPIO36) -> AO do sensor de umidade do solo
#define PIN_DIGITAL   5    // D5 (GPIO5)  -> DO do sensor
#define PIN_LED       2    // LED onboard (para receber comandos)

// --- CALIBRAÇÃO DO SENSOR ---
// Coloque MODO_CALIBRACAO como true, abra o Serial Monitor e anote:
//  - VALOR_SECO:    leitura com o sensor no ar / solo totalmente seco
//  - VALOR_MOLHADO: leitura com a ponta do sensor dentro de um copo d'água
// (os valores abaixo são apenas exemplos: ajuste para o seu sensor!)
#define MODO_CALIBRACAO false
const int VALOR_SECO    = 4095;
const int VALOR_MOLHADO = 2775;

// --- 2. CONFIGURAÇÕES DE REDE E MQTT ---
const char* ssid     = "Gio";          // SSID Wi-Fi
const char* password = "610v4nn4@";    // Senha Wi-Fi

// Credenciais do EMQX Cloud
const char* mqtt_server   = "k7e6191e.ala.us-east-1.emqxsl.com";
const int   mqtt_port     = 8883;           // Porta segura para TLS
const char* mqtt_user     = "Giovanna";     // Usuário criado em Access Control > Authentication
const char* mqtt_password = "610v4nn4";     // Senha desse usuário

// Tópicos de Publicação
const char* TOPIC_UMIDADE = "lab/solo/umidade";   // % (0 a 100)
const char* TOPIC_BRUTO   = "lab/solo/bruto";     // leitura ADC (0 a 4095)
const char* TOPIC_ESTADO  = "lab/solo/estado";    // "SECO" ou "UMIDO" (saída digital)

// Tópico de Assinatura (comando do LED)
const char* TOPIC_LED = "esp32/led";

// --- 3. CERTIFICADO CA (DigiCert Global Root CA - EMQX Cloud, validade 2031) ---
const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \
"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \
"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \
"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \
"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \
"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n" \
"-----END CERTIFICATE-----\n";

// --- 4. VARIÁVEIS DE CONTROLE ---
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
const long interval = 10000; // Publica dados a cada 10 segundos

// --- 5. LEITURA DO SENSOR (média de 10 amostras para reduzir ruído) ---
int lerUmidadeBruta() {
  long soma = 0;
  for (int i = 0; i < 10; i++) {
    soma += analogRead(PIN_ANALOGICO);
    delay(10);
  }
  return soma / 10;
}

// Converte leitura bruta em % (sensor resistivo: valor ALTO = solo SECO)
int bruto_para_percentual(int bruto) {
  int pct = map(bruto, VALOR_SECO, VALOR_MOLHADO, 0, 100);
  return constrain(pct, 0, 100);
}

// --- 6. CALLBACK (recebe mensagens) ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("]: ");

  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Liga ou desliga o LED
  if (message == "ON") {
    digitalWrite(PIN_LED, HIGH);
    Serial.println("LED ligado");
  } else if (message == "OFF") {
    digitalWrite(PIN_LED, LOW);
    Serial.println("LED desligado");
  }
}

// --- 7. CONEXÃO WI-FI ---
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

// --- 8. RECONEXÃO MQTT ---
void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexao MQTT...");
    if (client.connect("ESP32_Solo_Client", mqtt_user, mqtt_password)) {
      Serial.println("Conectado!");
      client.subscribe(TOPIC_LED);
    } else {
      Serial.print("Falha! rc=");
      Serial.print(client.state());
      char erroTLS[100];
      espClient.lastError(erroTLS, sizeof(erroTLS));
      Serial.print(" | TLS: ");
      Serial.print(erroTLS);
      Serial.println(" Tentando em 5 segundos...");
      delay(5000);
    }
  }
}

// --- 9. SETUP ---
void setup() {
  Serial.begin(115200);

  pinMode(PIN_DIGITAL, INPUT);
  pinMode(PIN_LED, OUTPUT);
  // GPIO36 (VP) é só entrada; não precisa de pinMode

  if (MODO_CALIBRACAO) {
    Serial.println("=== MODO CALIBRACAO (sem Wi-Fi/MQTT) ===");
    return;
  }

  setup_wifi();

  if (TESTE_SEM_CERTIFICADO) {
    espClient.setInsecure();            // SÓ PARA TESTE
    Serial.println("ATENCAO: modo teste, certificado NAO validado");
  } else {
    espClient.setCACert(ca_cert);
  }

  // Diagnóstico: o ESP32 consegue resolver o endereço do broker?
  IPAddress ipBroker;
  if (WiFi.hostByName(mqtt_server, ipBroker)) {
    Serial.print("DNS ok, broker em: ");
    Serial.println(ipBroker);
  } else {
    Serial.println("DNS FALHOU: sem internet nesse Wi-Fi?");
  }

  // Diagnóstico: testa conexão TCP simples (sem TLS) para ver se a porta passa
  {
    WiFiClient teste;
    Serial.print("TCP google.com:443 (controle) ... ");
    Serial.println(teste.connect("google.com", 443, 5000) ? "OK" : "FALHOU");
    teste.stop();

    Serial.print("TCP broker:8883 ................. ");
    Serial.println(teste.connect(ipBroker, mqtt_port, 5000) ? "ABERTA" : "BLOQUEADA / SEM RESPOSTA");
    teste.stop();
  }

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
}

// --- 10. LOOP PRINCIPAL ---
void loop() {
  // Modo de calibração: só imprime a leitura bruta a cada 1 s
  if (MODO_CALIBRACAO) {
    Serial.print("Leitura bruta: ");
    Serial.println(lerUmidadeBruta());
    delay(1000);
    return;
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantém a conexão MQTT ativa

  unsigned long now = millis();

  if (now - lastMsg > interval) {
    lastMsg = now;

    // --- 10.1 Leitura do sensor ---
    int bruto      = lerUmidadeBruta();
    int percentual = bruto_para_percentual(bruto);
    int digital    = digitalRead(PIN_DIGITAL);
    // Na maioria dos módulos LM393: DO = HIGH -> solo seco; LOW -> solo úmido
    // (se o seu for o contrário, inverta a condição abaixo)
    const char* estado = (digital == HIGH) ? "SECO" : "UMIDO";

    // --- 10.2 Publicação da UMIDADE (%) ---
    char pctPayload[8];
    snprintf(pctPayload, sizeof(pctPayload), "%d", percentual);
    client.publish(TOPIC_UMIDADE, pctPayload);
    Serial.print("Publicado [");
    Serial.print(TOPIC_UMIDADE);
    Serial.print("]: ");
    Serial.println(pctPayload);

    // --- 10.3 Publicação da LEITURA BRUTA ---
    char brutoPayload[8];
    snprintf(brutoPayload, sizeof(brutoPayload), "%d", bruto);
    client.publish(TOPIC_BRUTO, brutoPayload);
    Serial.print("Publicado [");
    Serial.print(TOPIC_BRUTO);
    Serial.print("]: ");
    Serial.println(brutoPayload);

    // --- 10.4 Publicação do ESTADO (saída digital) ---
    client.publish(TOPIC_ESTADO, estado);
    Serial.print("Publicado [");
    Serial.print(TOPIC_ESTADO);
    Serial.print("]: ");
    Serial.println(estado);
  }
}
