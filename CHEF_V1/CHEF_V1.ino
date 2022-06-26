#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
#define X_PIN 34
#define Y_PIN 35
int xValue, yValue;
int bossCheck = 0;


WiFiClient wifiClient;
PubSubClient client(wifiClient);

void writeCheck(int maId) {
  lcd.setCursor(4, 2);
  lcd.print(maId);
}

void bossChecker() {
  while (bossCheck == 0) {
    delay(100);
     client.loop();
    xValue = analogRead(X_PIN);
    Serial.println(xValue);
    if (xValue > 3500) {
      lcd.clear();
      lcd.setCursor(6, 2);
      bossCheck = 1;
      lcd.print("Genehmigt");
      sendBossRes(bossCheck);
    } else if (xValue < 1500) {
      lcd.clear();
      lcd.setCursor(6, 2);
      bossCheck = 2;
      lcd.print("Abgelehnt");
      sendBossRes(bossCheck);
    }
  }
  bossCheck = 0;
  delay (3000);
  printDefault();
}

// -----------------------------------------------------------------------------------------


/* WiFi-Data */
const char *ssid = "IoT-Netz";
const char *password = "CASIOT22";
WiFiMulti wifiMulti;

/* MQTT-Data */
const char *MQTTSERVER = "mq.jreichwald.de";
int MQTTPORT = 1883;
const char *mqttuser = "dbt";
const char *mqttpasswd = "dbt";
const char *mqttdevice = "ADCH";  // Please use a unique name here!
const char *outTopic = "bossResponse";


/* JSON-Document-Size for incoming JSON (object size may be increased for larger JSON files) */
const int capacity = JSON_OBJECT_SIZE(6);


/* Outgoing JSON Documents */
DynamicJsonDocument doc(capacity);


#define MSG_BUFFER_SIZE  (256) // Define the message buffer max size 
char msg[MSG_BUFFER_SIZE]; // Define the message buffer

/**
   This function is called when a MQTT-Message arrives.
*/
void mqtt_callback(char* topic, byte* payload, unsigned int length) {   //callback includes topic and payload ( from which (topic) the payload is comming)
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");

  // Create a JSON document on call stack
  StaticJsonDocument<capacity> doc;
  String jsonInput = String((char*)payload);

  // try to deserialize JSON
  DeserializationError err = deserializeJson(doc, jsonInput);

  // if an error occurs, print it out
  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
    return;
  }

  // Read out a name from JSON content (assuming JSON doc: {"SensorType" : "SomeSensorType", "value" : 42})
      Serial.print("-----------------------------------------------------------");
  int value = doc["maId"];
  Serial.println(value);
 //const char* name = doc["name"]; 
  writeCheck(value);
  bossChecker();
  Serial.println("Checkvorgang abgeschlossen");
}

/**
   This function is called from setup() and establishes a MQTT connection.
*/
void initMqtt() {
  client.setServer(MQTTSERVER, MQTTPORT);

  // Set the callback-Function when new messages are received.
  client.setCallback(mqtt_callback);


  client.connect(mqttdevice, mqttuser, mqttpasswd);
  while (!client.connected()) {
    Serial.print(".");
    delay(500);
  }

  // subscribe to a certain topic
  client.subscribe("waterLevel");
}

/**
   This function is called from setup() and establishes a WLAN connection
*/

void initWifi() {
  Serial.println("Connecting to WiFi ...");
  wifiMulti.addAP(ssid, password);

  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}



/**
   This function is called prior to sending data to mqtt.
   The JSON document gets cleared first (to free memory and
   avoid memory leaks), then sensor name, timestamp and
   measured values (humidity and temperature) are set to
   the JSON document.
*/
void setJSONData(int bossres) {
  doc.clear();
  doc["bossres"] = bossres;
}


// -----------------------------------------------------------------------------------------





void sendBossRes(int res) {
  Serial.println("Boss hat sich entschieden, sende Message an MA");
  // set measured data to preprared JSON document
  setJSONData(res);
  serializeJsonPretty(doc, msg);
  // Mesage an broker und warten auf Genehmigung

  // publish to MQTT broker
  client.publish(outTopic, msg);
  client.loop();
}


void printDefault() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Kaffeegenehmigung");
  lcd.setCursor(0, 1);
  lcd.print("Aktuelle Genehmigung");
}


void setup()
{
  Serial.begin(115200);

  //delay(2000);

  // Connect to WLAN
  initWifi();

  // Connect to MQTT server
  initMqtt();

  // Print to console
  Serial.println("Setup completed.");
  
  printDefault();
}


void loop()
{
  //Warten auf MQTT
  while(true){
    delay(100);
    client.loop();
  }

}
