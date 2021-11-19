#include "secrets.h"

//Include the required libraries
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoHA.h>
#include "DHT.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

WiFiManager wm;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET 4        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Define DHT sensor pin and type
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define UPDATEINTERVAL 10000

//Define variables
unsigned long lastReadAt = millis();
unsigned long lastTemperatureSend = millis();

float temperatureValue, humidityValue, signalstrengthValue;

// Rain level variables
#define RAINPIN 14
int rainLevel = 0;
const int AMOUNTOFRAINPERTICK = 8;       //Amount of rain per tick in mm
volatile unsigned long last_micros_rain; //Timer for debounce
// #define CLEARRAININTERVAL 3600000 // 1 hour for good measure
#define CLEARRAININTERVAL 300000 // 5 minutes for sake of the assignment
unsigned long rainLastCleared = millis();
volatile bool stateRainChanged = false;

// Windspeed variables
#define DEBOUNCE_TIME 15
#define ANEMOMETER_PIN 12 // Interrupt pin tied to anemometer Hall Effect Sensor

unsigned long lastWindSpeedSend = millis();
// const float KMPERHOURPERCYCLE = 2.4011; // for the pre made wind sensor
const float KMPERHOURPERCYCLE = 1.654246391; // for DIY wind meter

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

//Interrupt to increment the amount of rain
ICACHE_RAM_ATTR void registerRainTick()
{
    if ((long)(micros() - last_micros_rain) >= DEBOUNCE_TIME * 1000)
    {
        rainLevel += (AMOUNTOFRAINPERTICK / 2); //
        // sensorRain.setValue(rainLevel);
        stateRainChanged = true;
        last_micros_rain = micros();
    }
}

float calculateWindSpeed()
{
    //  rtc_wdt_feed();
    float real_speed = (anemometerCycles * KMPERHOURPERCYCLE);
    Serial.println("Amount of Cycles: " + String(anemometerCycles));
    sensorAnemometerPulses.setValue(anemometerCycles);
    anemometerCycles = 0;

    return real_speed;
}

void displaySensorData()
{
    display.clearDisplay();
    display.setCursor(0, 0);             // Start at top-left corner
    display.setTextSize(2);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.println("S-WEATHER");
    display.setTextSize(1);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text

    display.println("\nTemperature: " + String(temperatureValue) + " C");
    display.println("Humidity: " + String(humidityValue) + " %");
    display.println("Windspeed: " + String(windSpeed) + " Km/h");
    display.println("Rain level: " + String(rainLevel) + " mm");

    display.display();
}

void setup()
{
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    Serial.begin(9600);
    Serial.println("Starting...");

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }

    display.clearDisplay();

    display.setTextSize(2);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);             // Start at top-left corner
    display.println(F("S-Weather"));
    display.println(F("Sascha Vis"));
    display.println(F("0962873"));

    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.display();
    delay(2000); // Pause for 2 seconds

    // Clear the buffer
    display.clearDisplay();

    //reset settings - wipe credentials for testing
    //wm.resetSettings();

    wm.setConfigPortalBlocking(true);

    //automatically connect using saved credentials if they exist
    //If connection fails it starts an access point with the specified name
    if (wm.autoConnect("WeatherStation"))
    {
        Serial.println("connected...yeey :)");
    }
    else
    {
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
    Serial.println("\nConnected to S-weather MQTT broker");

    //configure the anemometer interrupt
    pinMode(ANEMOMETER_PIN, INPUT_PULLUP);
    pinMode(RAINPIN, INPUT_PULLUP);
    
    // Attach the interrupts
    attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), countAnemometer, RISING);
    attachInterrupt(digitalPinToInterrupt(RAINPIN), registerRainTick, RISING);

    sensorOwner.setValue(STUDENT_NAME);

    sensorLat.setValue(LAT, (uint8_t)15U);
    sensorLong.setValue(LONG, (uint8_t)15U);

    dht.begin();
}

void loop()
{
    wm.process();

    mqtt.loop();

    if ((millis() - rainLastCleared) > CLEARRAININTERVAL){
        rainLevel = 0; // reset the rain level
        sensorRain.setValue(rainLevel);
        Serial.println("Reset the rain level!");
        rainLastCleared = millis();
    }

    if ((millis() - lastWindSpeedSend) > UPDATEINTERVAL)
    { // read in 30ms interval

        humidityValue = dht.readHumidity();
        temperatureValue = dht.readTemperature();
        signalstrengthValue = WiFi.RSSI();

        if (isnan(humidityValue))
        {
            humidityValue = 0;
        }

        if (isnan(temperatureValue))
        {
            temperatureValue = 0;
        }

        if (stateRainChanged){
            sensorRain.setValue(rainLevel);
            stateRainChanged = false;
        }

        windSpeed = calculateWindSpeed(); // get amount of cycles from 2 seconds

        displaySensorData();

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
