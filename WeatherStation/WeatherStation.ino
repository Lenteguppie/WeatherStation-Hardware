/*
 * Author: Sascha Vis
 * Student number: 0962873
 * Date created: 07/09/2021
 * Last modified: 07/09/2021
 * Github: https://github.com/Lenteguppie/WeatherStation-Hardware
 * Board: ESP32
 */
#include "secrets.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <b64.h>
#include "DHT.h"
#include <ArduinoJson.h>

#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Define some variables we need later
float humidity;
float temperature;

int sensorPin = A0;
int moisture = 0;

int moisture_low = 650;
int moisture_high = 350;
int moisturePercentage = 0;

// My own numerical system for registering devices.
String student_number = STUDENT_NUMBER;
String mqttName = "HomeStation " + String(student_number);
String stateTopic = "home_station/weather/" + String(student_number) + "/state";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void sendSensorData() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  moisture = analogRead(sensorPin);

  if (isnan(humidity)) {
    humidity = 0;
  }

  if (isnan(temperature)) {
    temperature = 0;
  }

  // Map moisture sensor values to a percentage value
  moisturePercentage = map(moisture, moisture_low, moisture_high, 0, 100);

  DynamicJsonDocument doc(1024);
  char buffer[256];

  doc["humidity"] = humidity;
  doc["temperature"]   = temperature;
  doc["moisture"] = moisturePercentage;

  size_t n = serializeJson(doc, buffer);

  bool published = client.publish(stateTopic.c_str(), buffer, n);

  // Print the sensor values to Serial out (for debugging)
  Serial.println("published: ");
  Serial.println(published);
  Serial.println("humidity: ");
  Serial.println(humidity);
  Serial.println("temperature: ");
  Serial.println(temperature);
  Serial.println("moisture %: ");
  Serial.println(moisturePercentage);
}

void setup() {
  Serial.begin(9600);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.println("Connected to Wi-Fi");

  client.setServer(MQTT_BROKER, MQTT_PORT);

  Serial.println("Connecting to MQTT");

  while (!client.connected()) {
    Serial.print(".");

    if (client.connect(mqttName.c_str(), MQTT_USER, MQTT_PASSWORD)) {

      Serial.println("Connected to MQTT");

      sendMQTTTemperatureDiscoveryMsg();
      sendMQTTHumidityDiscoveryMsg();
      sendMQTTMoistureDiscoveryMsg();

    } else {

      Serial.println("failed with state ");
      Serial.print(client.state());
      delay(2000);

    }
  }

  dht.begin();
  // Go into deep sleep mode for 60 seconds
  //  Serial.println("Deep sleep mode for 60 seconds");
  //  ESP.deepSleep(10e6);
}

void sendMQTTTemperatureDiscoveryMsg() {
  Serial.println("Sending Temperature Discovery MSG");
  String discoveryTopic = "homeassistant/sensor/HomeStation_sensor_" + String(student_number) + "/temp/config";

  DynamicJsonDocument doc(1024);
  char buffer[256];

  doc["name"] = "Plant" + String(student_number) + " Temperature";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "°C";
  doc["dev_cla"] = "temperature";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.temperature|default(0) }}";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);

}

void sendMQTTHumidityDiscoveryMsg() {
  Serial.println("Sending Humidity Discovery MSG");
  String discoveryTopic = "homeassistant/sensor/homestation_sensor_" + String(student_number) + "/humidity/config";

  DynamicJsonDocument doc(1024);
  char buffer[256];

  doc["name"] = "Plant " + String(student_number) + " Humidity";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "%";
  doc["dev_cla"] = "humidity";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.humidity|default(0) }}";

  size_t n = serializeJson(doc, buffer);

  client.publish(discoveryTopic.c_str(), buffer, n);
}

void sendMQTTMoistureDiscoveryMsg() {
  Serial.println("Sending Moisture Discovery MSG");
  String discoveryTopic = "homeassistant/sensor/homestation_sensor_" + String(student_number) + "/moisture/config"; // The topic the sensor will post the entity configuration to.

  //Initialize the JSON Document
  DynamicJsonDocument doc(1024);
  char buffer[256];

  doc["name"] = "Homestation " + String(student_number) + " Moisture"; //The name of the sensor
  doc["stat_t"]   = stateTopic; //The topic the sensor will send the updates to
  doc["frc_upd"] = true; //Force update
  doc["val_tpl"] = "{{ value_json.moisture|default(0) }}"; //Value template. This will define how HASSIO will parse the data.

  size_t n = serializeJson(doc, buffer); //Convert the JSON document to a String

  client.publish(discoveryTopic.c_str(), buffer, n); //Publish the MQTT message to the discovery topic
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) { //Check if the WiFi is connected
    Serial.println("===== Sending Data =====");
    sendSensorData(); //Get the sensorData and send it over to the MQTT broker
  }
  else {
    Serial.println("WiFi Disconnected");
  }
  delay(10000);
}
