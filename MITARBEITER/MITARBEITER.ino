#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EasyBuzzer.h>


// ********** PARAMETERS START ***********
// GPIOs
#define POWER_PIN  16 // Wassersensor Power
#define SIGNAL_PIN 35 // Wassersensor Signal
#define LAUT 32 //Lautsprecher

// WiFi 
const char *ssid = "IoT-Netz";
const char *password = "CASIOT22";

//MQTT
const char *MQTTSERVER = "mq.jreichwald.de";
int MQTTPORT = 1883;
const char *mqttuser = "dbt";
const char *mqttpasswd = "dbt";
const char *mqttdevice = "MITARBEITER4711";  // Please use a unique name here!
const char *outTopic = "waterLevel";
#define MSG_BUFFER_SIZE  (256)
// ********** PARAMETERS END ***********

int waterLevel;
int maId = 4711;
int response = 0;
WiFiMulti wifiMulti;

void fillCoffee() {
  digitalWrite(21, HIGH);
  delay(1000);
  digitalWrite(21, LOW);
  delay(5000);
}

// -----------------------------------------------------------------------------------------


WiFiClient wifiClient;
PubSubClient client(wifiClient);

/* JSON-Document-Size for incoming JSON (object size may be increased for larger JSON files) */
const int capacity = JSON_OBJECT_SIZE(6);


/* Outgoing JSON Documents */
DynamicJsonDocument doc(capacity);

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
  //const char* name = doc["SensorType"];
  int res = doc["bossres"];
  int maIdC = doc["maIdRes"];
  // Serial.println(name);
  Serial.println("-------------------");
  Serial.println(res);
  if(maIdC == maId){
    // Antwort vom Chef auswerten und entsprechende Aktion durchführen
    if (res == 1) {
      // Kaffee nachfüllen
      fillCoffee();
    } else {
      // Ton abspielen
      Serial.println("Beeper");
      EasyBuzzer.singleBeep(200, 500);
      delay(1000);
      EasyBuzzer.stopBeep(); 
    }

    // Eine Stunde warten
    delay(3600000);
    response = 1;
  }
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
  client.subscribe("bossResponse");
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
void setJSONData(int id, int waterLevel) {
  doc.clear();
  doc["sensor"] = "WaterLevelSensor";
  doc["waterLevel"] = waterLevel;
  doc["maId"] = id;
}


// -----------------------------------------------------------------------------------------

 //Funktion, um den aktuellen Wasserstand im Kaffeebehälter zu lesen
int getWaterLevel() {
  int valueWL = 0;
  digitalWrite(POWER_PIN, HIGH);  // turn the sensor ON
  delay(10);                      // wait 10 milliseconds
  valueWL = analogRead(SIGNAL_PIN); // read the analog value from sensor
  digitalWrite(POWER_PIN, LOW);   // turn the sensor OFF
  Serial.print("The water sensor value: ");
  Serial.println(valueWL);
  return valueWL;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(21, OUTPUT);
  EasyBuzzer.setPin(LAUT);
  EasyBuzzer.setOnDuration(1000);
  pinMode(POWER_PIN, OUTPUT);   // configure pin as an OUTPUT
  digitalWrite(POWER_PIN, LOW); // turn the sensor OFF
  
  // Connect to WLAN
  initWifi();

  // Connect to MQTT server
  initMqtt();

  // Print to console
  Serial.println("Setup completed.");
}

void loop() {
  EasyBuzzer.update();
  client.loop();
  // Wasserstand im Kaffeebecher lesen
  waterLevel = getWaterLevel();

  if (waterLevel == 0) {
    client.loop();
    response = 0;
    Serial.println("WaterLevel == 0, sende Message an Boss");

    // Nachricht, dass Kaffee leer ist an Chef
    setJSONData(maId, waterLevel);
    serializeJsonPretty(doc, msg);
    client.publish(outTopic, msg);
    client.loop();

    while (response == 0) {
      // Auf Antwort vom Chef warten
      client.loop();
      delay(100);
    }
  }
  delay(3000)
}
