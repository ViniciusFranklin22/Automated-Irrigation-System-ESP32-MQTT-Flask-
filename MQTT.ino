/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-mqtt
 */


/*
Smart Plant Monitoring System
ESP32 + MQTT + Sensors

Features:
- Soil humidity monitoring
- Automatic irrigation
- Remote irrigation via MQTT
- Configurable thresholds
- Real time telemetry
- NTP timestamp
- WiFi + MQTT auto reconnection
*/

#include <WiFi.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <time.h>
//#include <bits/stdc++.h>
#include <DHT11.h>
#include <Preferences.h>
Preferences prefs;


struct DHTData {
  int temperatura;
  int umidade;
};

// ===== PROTOTIPOS DAS FUNCOES =====

void connectToMQTT();
float get_ldr_value();
DHTData get_dht11_values();
float get_soil_humidity();
void regar_vaso_auto();
void check_threshold();
void sendToMQTT();
void messageHandler(String &topic, String &payload);
String get_real_time();
void regar_vaso_manual();
void reconnectToWiFi();
void ligar_bomba();
void create_var_flash();
void setUIntFromPayload(const char* key, String payload, uint &targetVar);

#define CLIENT_ID "ESP32-001"  // CHANGE IT AS YOU DESIRE

#define D_TEMP_PIN 33
#define DHT_READ_TIME 100000


DHT11 dht11(D_TEMP_PIN);
const char WIFI_SSID[] = "";              // CHANGE TO YOUR WIFI SSID
const char WIFI_PASSWORD[] = "";           // CHANGE TO YOUR WIFI PASSWORD
const char MQTT_BROKER_ADRRESS[] = "";  // CHANGE TO MQTT BROKER'S IP ADDRESS
const int MQTT_PORT = 1883;
const char MQTT_USERNAME[] = "";  // CHANGE IT IF REQUIRED
const char MQTT_PASSWORD[] = "";  // CHANGE IT IF REQUIRED
volatile bool flag = false;
String hora_ultima_irrigacao="0";
DHTData dht_cache;
unsigned long lastDHTRead = 0;
long timezone = -3;
byte daysavetime = 0;

// The MQTT topics that this device should publish/subscribe
unsigned long lastAutoCheck = 0;
int AUTO_CHECK_INTERVAL = 10000; // 10 segundos
bool irrigacao_em_curso = 0;

//#define LM75_ADDRESS 0x48  // Endereço padrão do LM75
//#define TEMP_REGISTER 0x00 // Registro de leitura de temperatura
#define PUBLISH_TOPIC "esp32-001/send"
#define INICIO_IRRIGACAO_PUBLISH_TOPIC "esp32-001/inicio_irrigacao"
#define SUBSCRIBE_TOPIC "esp32-001/receive"
#define TOPIC_CONFIG_THSHOLD "esp32-001/thshold"
#define TOPIC_CONFIG_MIN_ENTRE_IRRIGACAO "esp32-001/min_entre_irrigacao"
#define TOPIC_CONFIG_TEMPO_IRRIGACAO "esp32-001/tempo_irrigacao"
#define TOPIC_BOOL_IRRIGACAO_AUTO "esp32-001/auto_mode"
#define PINO_LED_WIFI 16 
#define PINO_LED_MQTT 17 
#define RW_MODE false
#define RO_MODE true
#define A0_LDR_PIN 36  
#define A1_HUM_PIN 39
//#define I2C_SDA 21  // SDA padrão no ESP32 DevKit v1
//#define I2C_SCL 22  // SCL padrão no ESP32 DevKit v1
#define PUBLISH_INTERVAL 1000  // 1 seconds
//#define TEMPO_RELE_ATIVO 5000 // tempo para irrigacao, era bom setar via MQTT, da liberdade ao user
#define RelePin  4 // pino ao qual o Módulo Relé está conectado
//#define tempo_min_entre_irrigacoes 1000 * 60 * 60 * 4 // era bom setar via MQTT, da liberdade ao user

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);

// serao setadas a partir da memoria flash
bool irrig_auto; 
uint threshold_humidity; // so para testes como nao ta conectado a nada, era bom setar via MQTT, da liberdade ao user
uint TEMPO_RELE_ATIVO ;
uint tempo_min_entre_irrigacoes;

void create_var_flash(){


  prefs.begin("config", RO_MODE);           // Open our namespace (or create it if it doesn't exist) in RO mode.

  bool tpInit = prefs.isKey("nvsInit");       // Test for the existence of the "already initialized" key.
                                                
  if (tpInit == false) {
    
    prefs.end();
    prefs.begin("config", RW_MODE);
    // salvar valores de fabrica

    prefs.putBool("irrig_auto",0);
    prefs.putUInt("threshold_humidity",4000);
    prefs.putUInt("TEMPO_RELE_ATIVO",5000);
    prefs.putUInt("tempo_min_entre_irrigacoes",14400000); // 1000 * 60 * 60 * 4
    prefs.putBool("nvsInit", true); 
    prefs.end();
    prefs.begin("config", RO_MODE); 

  }

  irrig_auto = prefs.getBool("irrig_auto"); // flash
  threshold_humidity = prefs.getUInt("threshold_humidity"); // so para testes como nao ta conectado a nada, era bom setar via MQTT, da liberdade ao user
  TEMPO_RELE_ATIVO = prefs.getUInt("TEMPO_RELE_ATIVO");
  tempo_min_entre_irrigacoes = prefs.getUInt("tempo_min_entre_irrigacoes");
  // All done. Last run state (or the factory default) is now restored.
  prefs.end();

}

unsigned long ultima_irrigacao = 0;
unsigned long lastPublishTime = 0;

//float threshold_humidity = 2500;  // valor ADC para iniciar irrigação
int soil_cache;

void setup() {
  Serial.begin(115200);

  create_var_flash();

  //Wire.begin(I2C_SDA, I2C_SCL);  //  , SCL = GPIO 22 padrão no ESP32
  pinMode(RelePin, OUTPUT); // seta o pino como saída
  digitalWrite(RelePin, LOW); // seta o pino com nivel logico baixo

  pinMode(PINO_LED_WIFI,OUTPUT);
  pinMode(PINO_LED_MQTT,OUTPUT);
  digitalWrite(PINO_LED_WIFI,LOW);
  digitalWrite(PINO_LED_MQTT,LOW);

  // set the ADC attenuation to 11 dB (up to ~3.3V input)
  analogSetAttenuation(ADC_11db);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);


  Serial.println("ESP32 - Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    
    Serial.print(".A");
    digitalWrite(PINO_LED_WIFI,LOW);
    delay(500);
  }
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(PINO_LED_WIFI,HIGH);
  //Serial.print("Ping broker: ");
  //Serial.println(WiFi.ping("192.168.0.174"));


  Serial.println();

  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);
  // Create a handler for incoming messages
  mqtt.onMessage(messageHandler);

  connectToMQTT();

  Serial.println("Contacting Time Server");
  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");

}



void reconnectToWiFi() {

  if (WiFi.status() == WL_CONNECTED){
    digitalWrite(PINO_LED_WIFI,HIGH);
    return;
    }

  Serial.println("Reconectando ao WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".W");
    digitalWrite(PINO_LED_WIFI,LOW);
    delay(500);
  }
  digitalWrite(PINO_LED_WIFI,HIGH);
  Serial.println("\nWiFi reconectado!");
}

void connectToMQTT() {
  // Connect to the MQTT broker
  if (mqtt.connected()) {
    digitalWrite(PINO_LED_MQTT,HIGH);
    return;
    }
  

  Serial.print("ESP32 - Connecting to MQTT broker");

  while (!mqtt.connect(CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD) && WiFi.status() == WL_CONNECTED) {
    Serial.print(".B");
    digitalWrite(PINO_LED_MQTT,LOW);
    delay(100);
    }

  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("ESP32 - MQTT broker Timeout!");
    return;
  }

  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(SUBSCRIBE_TOPIC)) {
      Serial.print("ESP32 - Subscribed to the topic: ");
      Serial.println(SUBSCRIBE_TOPIC);
  } else {
      Serial.print("ESP32 - Failed to subscribe to the topic: ");
}
    // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(TOPIC_CONFIG_THSHOLD  )){
    Serial.print("ESP32 - Subscribed to the topic: ");
    Serial.println(TOPIC_CONFIG_THSHOLD);
    }
  else
    Serial.print("ESP32 - Failed to subscribe to the topic: ");

  

    // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(TOPIC_CONFIG_MIN_ENTRE_IRRIGACAO )){
    Serial.print("ESP32 - Subscribed to the topic: ");
    Serial.println(TOPIC_CONFIG_MIN_ENTRE_IRRIGACAO);
  }
  else
    Serial.print("ESP32 - Failed to subscribe to the topic: ");


    // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(TOPIC_CONFIG_TEMPO_IRRIGACAO)){
    Serial.print("ESP32 - Subscribed to the topic: ");
    Serial.println(TOPIC_CONFIG_TEMPO_IRRIGACAO);
  }
  else
    Serial.print("ESP32 - Failed to subscribe to the topic: ");

    // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(TOPIC_BOOL_IRRIGACAO_AUTO)){
    Serial.print("ESP32 - Subscribed to the topic: ");
    Serial.println(TOPIC_BOOL_IRRIGACAO_AUTO);
  }
  else
    Serial.print("ESP32 - Failed to subscribe to the topic: ");

  
  Serial.println("ESP32  - MQTT broker Connected!");
  digitalWrite(PINO_LED_MQTT,HIGH);
}

float get_ldr_value() {
  int samples = 5;
  long sum = 0;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(A0_LDR_PIN);
    //delay(5);  // opcional, para dar tempo entre leituras
  }

  float media = sum / (float)samples;
  return media;
}



void update_dht() {

  if (millis() - lastDHTRead < 2000) return;

  int temperature = 0;
  int humidity = 0;

  if (dht11.readTemperatureHumidity(temperature, humidity) == 0) {
      dht_cache.temperatura = temperature;
      dht_cache.umidade = humidity;
  }

  lastDHTRead = millis();
}

float get_soil_humidity(){
  int samples = 5;
  long sum = 0;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(A1_HUM_PIN);
    //delay(5);  // opcional, para dar tempo entre leituras
  }

  float media = sum / (float)samples;
  return media;
}




void sendToMQTT() {
  StaticJsonDocument<300> message;
  String timestamp = get_real_time(); 
  soil_cache = get_soil_humidity();
  int ldr_value = get_ldr_value();
  int temp_value = dht_cache.temperatura;
  int umid_value = dht_cache.umidade;

  message["timestamp"] = timestamp;
  message["soil_humidity"] = soil_cache;
  message["ldr_value"] = ldr_value;
  message["temp_value"] = temp_value;
  message["umid_value"] = umid_value;
  message["ultima_irrigacao"] = hora_ultima_irrigacao;
  message["threshold_humidity"] = threshold_humidity;
  message["TEMPO_RELE_ATIVO"] = TEMPO_RELE_ATIVO;
  message["tempo_min_entre_irrigacoes"] = tempo_min_entre_irrigacoes;
  message["bool_irrigacao_auto"] = irrig_auto;
  //message["irrigacao_em_curso"] = irrigacao_em_curso;

  char messageBuffer[384];
  serializeJson(message, messageBuffer);

  mqtt.publish(PUBLISH_TOPIC, messageBuffer);  // terceiro parâmetro = retain


  Serial.println("ESP32 - sent to MQTT:");
  Serial.print("- topic: ");
  Serial.println(PUBLISH_TOPIC);
  Serial.print("- payload:");
  Serial.println(messageBuffer);
  

}


void messageHandler(String &topic, String &payload) {
  Serial.println("ESP32 - received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);

  
  if (topic == TOPIC_CONFIG_THSHOLD) {
      setUIntFromPayload("threshold_humidity", payload, threshold_humidity);
  }
  else if (topic == TOPIC_CONFIG_MIN_ENTRE_IRRIGACAO) {
      setUIntFromPayload("tempo_min_entre_irrigacoes", payload, tempo_min_entre_irrigacoes);
  }
  else if (topic == TOPIC_CONFIG_TEMPO_IRRIGACAO) {
      setUIntFromPayload("TEMPO_RELE_ATIVO", payload, TEMPO_RELE_ATIVO);
  }
  else if (topic == SUBSCRIBE_TOPIC) {
    if (payload == "Regar Vaso Agora") {
      regar_vaso_manual();

    }
  }
  else if (topic == TOPIC_BOOL_IRRIGACAO_AUTO) {
    irrig_auto = (payload == "1");
    prefs.begin("config", RW_MODE);
    prefs.putBool("irrig_auto",irrig_auto);
    prefs.end();
  }


}





String get_real_time(){
  char dateBuffer[20];
  char timeBuffer[20];
  char timestamp[40];

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    
    strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%Y", &timeinfo);
    
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  /*
    Serial.print("Date: ");
    Serial.print(dateBuffer);
    Serial.print(" - Time: ");
    Serial.println(timeBuffer);
    */
  } else {
    Serial.println("Failed to get time");
  }
  sprintf(timestamp, "%s %s", dateBuffer, timeBuffer);

  return timestamp;


}


void regar_vaso_auto() {
  if (millis() - ultima_irrigacao >= tempo_min_entre_irrigacoes) {  // Corrigido: >= e não <=
    ligar_bomba();
  } else {
    Serial.println("Irrigação ignorada: tempo mínimo ainda não passou.");
  }
  
}

void check_threshold(){
  if(soil_cache >= threshold_humidity){
    regar_vaso_auto();
  }

}
void regar_vaso_manual() {
  ligar_bomba();
  
}

void ligar_bomba(){
    Serial.println("Iniciando irrigação...");
    irrigacao_em_curso = 1;
    String timestamp_inicio = get_real_time(); 
    sendToMQTT_Irrigacao(timestamp_inicio,"",0);
		digitalWrite(RelePin, HIGH); // Liga relé (pode ser LOW dependendo do módulo!)
		delay(TEMPO_RELE_ATIVO);    // Espera o tempo de irrigação
		digitalWrite(RelePin, LOW); // Desliga relé
    irrigacao_em_curso = 0;
    String timestamp_fim = get_real_time(); 
    sendToMQTT_Irrigacao(timestamp_inicio,timestamp_fim,1); 
		Serial.println("Irrigação finalizada.");
		hora_ultima_irrigacao = get_real_time(); 
		ultima_irrigacao = millis(); // Atualiza tempo da última irrigação
		mqtt.publish(SUBSCRIBE_TOPIC, "1");
}



void setUIntFromPayload(const char* key, String payload, uint &targetVar) {
    targetVar = strtoul(payload.c_str(), NULL, 10); // converte para uint
    prefs.begin("config", RW_MODE);
    prefs.putUInt(key, targetVar);
    prefs.end();
}

void sendToMQTT_Irrigacao(String timestamp_inicio, String timestamp_fim,bool finalizada) {
  StaticJsonDocument<200> message;
  

  message["timestamp_inicio"] = timestamp_inicio;
  message["irrigacao_em_curso"] = irrigacao_em_curso;
  message["timestamp_fim"] = timestamp_fim;
  message["irrigacao_finalizada"] = finalizada;

  char messageBuffer[256];
  serializeJson(message, messageBuffer);

  mqtt.publish(INICIO_IRRIGACAO_PUBLISH_TOPIC, messageBuffer);  // terceiro parâmetro = retain


  Serial.println("ESP32 - sent to MQTT:");
  Serial.print("- topic: ");
  Serial.println(PUBLISH_TOPIC);
  Serial.print("- payload:");
  Serial.println(INICIO_IRRIGACAO_PUBLISH_TOPIC);
  

}
void loop() {



  reconnectToWiFi();

  if (!mqtt.connected() && WiFi.status() == WL_CONNECTED ) {
    connectToMQTT();
  }


  mqtt.loop();


  update_dht();
  if (millis() - lastPublishTime >= PUBLISH_INTERVAL) {
      sendToMQTT();
      lastPublishTime = millis();
  }

if (irrig_auto && millis() - lastAutoCheck >= AUTO_CHECK_INTERVAL) {
    check_threshold();
    lastAutoCheck = millis();
}
  
}
