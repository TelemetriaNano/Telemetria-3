/*
 SCADA NANO v2.0 - VERSION COMPATIBLE DASHBOARD
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

// ===== CONFIGURACIÓN =====
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASS ""

#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_CLIENT "ESP32_SCADA_CLIENT"

#define DEVICE_ID "ESP32_CLIENTE001_01"

// PINES
#define PIN_POT1 34
#define PIN_POT2 35

#define PIN_LED_BOMBA1 25
#define PIN_LED_BOMBA2 26
#define PIN_LED_ALARM 27
#define PIN_SERVO 18

#define PIN_SWITCH1 32
#define PIN_SWITCH2 33

#define TELEMETRY_INTERVAL 3000

// ===== OBJETOS =====
WiFiClient espClient;
PubSubClient mqtt(espClient);
Servo servoValvula;

// ===== VARIABLES =====
unsigned long lastTelemetry = 0;

bool bomba1State=false;
bool bomba2State=false;
int servoAngle=0;


// =====================================================
// WIFI
// =====================================================
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi OK");
}


// =====================================================
// MQTT CALLBACK
// =====================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  String msg;
  for (int i=0;i<length;i++) msg += (char)payload[i];

  Serial.println("RX: "+String(topic)+" -> "+msg);

  String expected = String("esp32/")+DEVICE_ID+"/command";
  if(String(topic) != expected) return;

  StaticJsonDocument<200> doc;
  if(deserializeJson(doc,msg)) return;

  String command = doc["command"];
  String value   = doc["value"];

  if(command=="bomba1"){
    bomba1State = (value=="1");
    digitalWrite(PIN_LED_BOMBA1,bomba1State);
  }

  else if(command=="bomba2"){
    bomba2State = (value=="1");
    digitalWrite(PIN_LED_BOMBA2,bomba2State);
  }

  else if(command=="valvula"){
    servoAngle = constrain(value.toInt(),0,180);
    servoValvula.write(servoAngle);
  }
}


// =====================================================
// MQTT CONNECT
// =====================================================
void connectMQTT(){

  while(!mqtt.connected()){
    Serial.print("Conectando MQTT...");

    if(mqtt.connect(MQTT_CLIENT)){
      Serial.println("OK");

      String cmdTopic = String("esp32/")+DEVICE_ID+"/command";
      mqtt.subscribe(cmdTopic.c_str());

      mqtt.publish("esp32/system/status","online",true);
    }
    else{
      Serial.println("fail");
      delay(2000);
    }
  }
}


// =====================================================
// SETUP
// =====================================================
void setup(){

  Serial.begin(115200);

  pinMode(PIN_LED_BOMBA1,OUTPUT);
  pinMode(PIN_LED_BOMBA2,OUTPUT);
  pinMode(PIN_LED_ALARM,OUTPUT);

  pinMode(PIN_SWITCH1,INPUT);
  pinMode(PIN_SWITCH2,INPUT);

  servoValvula.attach(PIN_SERVO);
  servoValvula.write(0);

  connectWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}


// =====================================================
// TELEMETRIA JSON
// =====================================================
void sendTelemetry(){

  if(millis()-lastTelemetry < TELEMETRY_INTERVAL) return;

  int nivel = map(analogRead(PIN_POT1),0,4095,0,100);
  int presion = map(analogRead(PIN_POT2),0,4095,0,100);

  bool puerta = digitalRead(PIN_SWITCH1);
  bool sensorNivel = digitalRead(PIN_SWITCH2);

  StaticJsonDocument<256> doc;

  doc["nivel"] = nivel;
  doc["presion"] = presion;
  doc["puerta"] = puerta;
  doc["sensor_nivel"] = sensorNivel;

  String payload;
  serializeJson(doc,payload);

  String topic = String("esp32/")+DEVICE_ID+"/telemetry";

  mqtt.publish(topic.c_str(),payload.c_str());

  lastTelemetry = millis();
}


// =====================================================
// LOOP
// =====================================================
void loop(){

  if(!mqtt.connected())
    connectMQTT();

  mqtt.loop();

  sendTelemetry();
}
