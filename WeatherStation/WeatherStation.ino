/*
 * Author: Sascha Vis
 * Student number: 0962873
 * Date created: 07/09/2021
 * Last modified: 07/09/2021
 * Github: https://github.com/Lenteguppie/WeatherStation-Hardware
 */


#include "DHT.h"

#define DHTPIN 2     // Digital GPIO pin connected to the DHT sensor

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

//initialize the DHT11 module
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  dht.begin();
}

float getTemperature(){
   return dht.readTemperature();
}

float getHumidity(){
  return dht.readHumidity();
}

float getWindSpeed(){
  //TODO: Get wind speed.
}

float getWindDir(){
  //TODO: Get wind direction.
}

float getBatteryPercentage(){
  //TODO: Get battery percentage.
}

void loop() {
  //TODO: Connect to WiFi.
  //TODO: Maintain WiFi connection. 

  //TODO: Connect to MQTT.
  //TODO: Publish to MQTT broker.
  //TODO: Send HTTP post request.

  //TODO Build in deep sleep functionality.
}
