#include "secrets.h"

//Include the required libraries
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoHA.h>
#include "DHT.h"

WiFiManager wm;

//Define DHT sensor pin and type
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define UPDATEINTERVAL 10000

//Define variables
unsigned long lastReadAt = millis();
unsigned long lastTemperatureSend = millis();
bool lastInputState = false;

float temperatureValue, humidityValue, signalstrengthValue;

int rainLevel = 0;

// Windspeed variables
#define DEBOUNCE_TIME 15
#define ANEMOMETER_PIN 27 // Interrupt pin tied to anemometer reed switch

unsigned long lastWindSpeedSend = millis();
const float KMPERHOURPERCYCLE = 2.4011;
float windSpeed = 0.0;
volatile int anemometerCycles;
volatile unsigned long last_micros_an; //Timer for debounce

//Initialize WiFi
WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);

//Define the sensors and/or devices
//The string must not contain any spaces!!! Otherwise the sensor will not show up in Home Assistant
HASensor sensorOwner("Owner");
HASensor sensorLong("Long");
HASensor sensorLat("Lat");
HASensor sensorTemperature("Temperature");
HASensor sensorHumidity("Humidity");
HASensor sensorRain("RegenMeter");
HASensor sensorWindSPD("Wind_speed");
HASensor sensorAnemometerPulses("Anemometer_pulses");
HASensor sensorSignalstrength("Signal_strength");

//Interrupt to increment the anemometer counter
ICACHE_RAM_ATTR void countAnemometer()
{
  if ((long)(micros() - last_micros_an) >= DEBOUNCE_TIME * 1000)
  {
    anemometerCycles++;
//    Serial.println("Pulse...");
    last_micros_an = micros();
  }
}

float calculateWindSpeed()
{
//  rtc_wdt_feed();
  float real_speed = (anemometerCycles * (KMPERHOURPERCYCLE * 1000)) / UPDATEINTERVAL;
  Serial.println("Amount of Cycles: " + String(anemometerCycles));
  sensorAnemometerPulses.setValue(anemometerCycles);
  anemometerCycles = 0;

  return real_speed;
}
void setup()
{
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  Serial.begin(9600);
  Serial.println("Starting...");

  //reset settings - wipe credentials for testing
  //wm.resetSettings();

  wm.setConfigPortalBlocking(true);

  //automatically connect using saved credentials if they exist
  //If connection fails it starts an access point with the specified name
  if (wm.autoConnect("WeatherStation")) {
    Serial.println("connected...yeey :)");
  }
  else {
    Serial.println("Configportal running");
  }

  // Unique ID must be set!
  byte mac[6];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));

  // Connect to wifi
  //  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //  while (WiFi.status() != WL_CONNECTED)
  //  {
  //    Serial.print(".");
  //    delay(500); // waiting for the connection
  //  }
  //  Serial.println();
  //  Serial.println("Connected to the network");

  // Set sensor and/or device names
  // String conversion for incoming data from Secret.h
  String student_id = STUDENT_ID;
  String student_name = STUDENT_NAME;

  //Add student ID number with sensor name
  String stationNameStr = student_name + "'s Home Station";
  String ownerNameStr = student_id + " Station owner";
  String longNameStr = student_id + " Long";
  String latNameStr = student_id + " Lat";
  String temperatureNameStr = student_id + " Temperature";
  String humidityNameStr = student_id + " Humidity";
  String signalstrengthNameStr = student_id + " Signal Strength";
  String rainNameStr = student_id + " Rain Level";
  String windSensorStr = student_id + " Wind Speed";
  String anemometerPulseNameSTR = student_id + " Anemometer";
  
  //Convert the strings to const char*
  const char *stationName = stationNameStr.c_str();
  const char *ownerName = ownerNameStr.c_str();
  const char *longName = longNameStr.c_str();
  const char *latName = latNameStr.c_str();
  const char *temperatureName = temperatureNameStr.c_str();
  const char *humidityName = humidityNameStr.c_str();
  const char *signalstrengthName = signalstrengthNameStr.c_str();
  const char *rainName = rainNameStr.c_str();
  const char *windName = windSensorStr.c_str();
  const char *anemometerPulseName = anemometerPulseNameSTR.c_str();

  //Set main device name
  device.setName(stationName);
  device.setSoftwareVersion(SOFTWARE_VERSION);
  device.setManufacturer(STUDENT_NAME);
  device.setModel(MODEL_TYPE);

  sensorRain.setName(rainName);
  sensorRain.setIcon("mdi:weather-rainy");
  sensorRain.setUnitOfMeasurement("mm");

  sensorWindSPD.setName(windName);
  sensorWindSPD.setIcon("mdi:cloud");
  sensorWindSPD.setUnitOfMeasurement("km/h");

  sensorAnemometerPulses.setName(anemometerPulseName);
  sensorAnemometerPulses.setUnitOfMeasurement("pulses");

  sensorOwner.setName(ownerName);
  sensorOwner.setIcon("mdi:account");

  sensorLong.setName(longName);
  sensorLong.setIcon("mdi:crosshairs-gps");
  sensorLat.setName(latName);
  sensorLat.setIcon("mdi:crosshairs-gps");

  sensorTemperature.setName(temperatureName);
  sensorTemperature.setDeviceClass("temperature");
  sensorTemperature.setUnitOfMeasurement("°C");

  sensorHumidity.setName(humidityName);
  sensorHumidity.setDeviceClass("humidity");
  sensorHumidity.setUnitOfMeasurement("%");

  sensorSignalstrength.setName(signalstrengthName);
  sensorSignalstrength.setDeviceClass("signal_strength");
  sensorSignalstrength.setUnitOfMeasurement("dBm");

  mqtt.begin(BROKER_ADDR, BROKER_USERNAME, BROKER_PASSWORD);

  while (!mqtt.isConnected())
  {
    mqtt.loop();
    Serial.print(".");
    delay(500); // waiting for the connection
  }

  Serial.println();
  Serial.println("Connected to MQTT broker");

  //configure the anemometer interrupt
  pinMode(ANEMOMETER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), countAnemometer, RISING);

  sensorOwner.setValue(STUDENT_NAME);

  sensorLat.setValue(LAT, (uint8_t)15U);
  sensorLong.setValue(LONG, (uint8_t)15U);

  dht.begin();
}

void loop()
{
  wm.process();

  mqtt.loop();

  if ((millis() - lastWindSpeedSend) > UPDATEINTERVAL)
  { // read in 30ms interval

    humidityValue = dht.readHumidity();
    temperatureValue = dht.readTemperature();
    signalstrengthValue = WiFi.RSSI();

    if (isnan(humidityValue)) {
      humidityValue = 0;
    }

    if (isnan(temperatureValue)) {
      temperatureValue = 0;
    }



    windSpeed = calculateWindSpeed(); // get amount of cycles from 2 seconds

    //TODO: send data to HA
    sensorTemperature.setValue(temperatureValue);
    Serial.print("Current temperature is: ");
    Serial.print(temperatureValue);
    Serial.println("°C");

    sensorHumidity.setValue(humidityValue);
    Serial.print("Current humidity is: ");
    Serial.print(humidityValue);
    Serial.println("%");

    sensorSignalstrength.setValue(signalstrengthValue);
    Serial.print("Current signal strength is: ");
    Serial.print(signalstrengthValue);
    Serial.println("%");

    sensorRain.setValue(rainLevel);
    Serial.print("Current rain level is: ");
    Serial.print(rainLevel);
    Serial.println("mm");

    sensorWindSPD.setValue(windSpeed);
    Serial.print("Current wind speed is: ");
    Serial.print(windSpeed);
    Serial.println("km/h");

    lastWindSpeedSend = millis();
  }
}
