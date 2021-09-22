/*
   Author: Sascha Vis
   Student number: 0962873
   Date created: 07/09/2021
   Last modified: 07/09/2021
   Github: https://github.com/Lenteguppie/WeatherStation-Hardware
   Board: ESP32
*/
#include "secrets.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <b64.h>
#include "DHT.h"
#include <ArduinoJson.h>

//Deep sleep Constants
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */

//DHT11 Constants
#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Define some variables we need later
float humidity;
float temperature;

float wind_speed;

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

  wind_speed = 0;

  // Map moisture sensor values to a percentage value
  moisturePercentage = map(moisture, moisture_low, moisture_high, 0, 100);

  DynamicJsonDocument doc(1024);
  char buffer[256];
  doc["windspeed"] = wind_speed;
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
  delay(500); //Get some time to start up the serial monitor

  /*
    First we configure the wake up source
    We set our ESP32 to wake up every 5 seconds
  */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");

  //Start the WiFi connection
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
  
  bool resized = client.setBufferSize(512); //resizes the buffer because the discovery payloads can be pretty big
  Serial.println("resized buffer: " + String(resized));

  while (!client.connected()) {
    Serial.print(".");

    if (client.connect(mqttName.c_str(), MQTT_USER, MQTT_PASSWORD)) {

      Serial.println("Connected to MQTT");

      sendMQTTTemperatureDiscoveryMsg();
      delay(500);
      sendMQTTHumidityDiscoveryMsg();
      delay(500);
      sendMQTTMoistureDiscoveryMsg();
      delay(500);
      sendMQTTWindSpeedDiscoveryMsg();
      delay(500);

      //Start the DHT
      dht.begin();

      Serial.println("===== Sending Data =====");
      sendSensorData(); //Get the sensorData and send it over to the MQTT broker

    } else {

      Serial.println("failed with state ");
      Serial.print(client.state());
      delay(2000);

    }
  }

  //Go into deep sleep...
  Serial.println("Going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void sendMQTTTemperatureDiscoveryMsg() {
  Serial.println("Sending Temperature Discovery MSG");
  String discoveryTopic = "homeassistant/sensor/homestation_sensor_" + String(student_number) + "/temperature/config";

  DynamicJsonDocument doc(1024);
  char buffer[256];

  doc["name"] = "Homestation" + String(student_number) + " Temperature";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "°C";
  doc["dev_cla"] = "temperature";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.temperature|default(0) }}";

  size_t n = serializeJson(doc, buffer);

  bool sent = client.publish(discoveryTopic.c_str(), buffer, n);
  Serial.println("Send to topic: " + String(sent));

}

void sendMQTTHumidityDiscoveryMsg() {
  Serial.println("Sending Humidity Discovery MSG");
  String discoveryTopic = "homeassistant/sensor/homestation_sensor_" + String(student_number) + "/humidity/config";

  DynamicJsonDocument doc(1024);
  char buffer[256];

  doc["name"] = "Homestation " + String(student_number) + " Humidity";
  doc["stat_t"]   = stateTopic;
  doc["unit_of_meas"] = "%";
  doc["dev_cla"] = "humidity";
  doc["frc_upd"] = true;
  doc["val_tpl"] = "{{ value_json.humidity|default(0) }}";

  size_t n = serializeJson(doc, buffer);

  bool sent = client.publish(discoveryTopic.c_str(), buffer, n);
  Serial.println("Send to topic: " + String(sent));
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
  
  bool sent = client.publish(discoveryTopic.c_str(), buffer, n); //Publish the MQTT message to the discovery topic
  Serial.println("Send to topic: " + String(sent));
}

void sendMQTTWindSpeedDiscoveryMsg() {
  Serial.println("Sending Wind Speed Discovery MSG");
  String discoveryTopic = "homeassistant/sensor/homestation_sensor_" + String(student_number) + "/wind_speed/config"; // The topic the sensor will post the entity configuration to.

  //Initialize the JSON Document
  DynamicJsonDocument doc(1024);
  char buffer[256];

  doc["name"] = "Homestation " + String(student_number) + " Wind Speed"; //The name of the sensor
  doc["stat_t"]   = stateTopic; //The topic the sensor will send the updates to
  doc["dev_cla"] = "speed";
  doc["unit_of_meas"] = "m/s";
  doc["frc_upd"] = true; //Force update
  doc["val_tpl"] = "{{ value_json.windspeed|default(0) }}"; //Value template. This will define how HASSIO will parse the data.

  size_t n = serializeJson(doc, buffer); //Convert the JSON document to a String

  bool sent = client.publish(discoveryTopic.c_str(), buffer, n); //Publish the MQTT message to the discovery topic
  Serial.println("Send to topic: " + String(sent));
}

void loop() {
  //This is not going to be used
}
