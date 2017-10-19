//
// Using Arduino Nano
//
#include "Timer.h"              // Timer library - https://github.com/JChristensen/Timer
#include <OneWire.h>            // I2C Bus onewire (Standard lib)
#include <DallasTemperature.h>  // Temperature sensor (Standard lib)
#include <RH_ASK.h>

RH_ASK driver(1000, A1, 12, A2); // pin 12

const int ldr = A7;
const int led_pin = 3;


// ############################################################################
// # Temperature Sensor DS18B20
#define DATA_PIN 2  // Pin
// How many bits to use for temperature values: 9, 10, 11 or 12
#define SENSOR_RESOLUTION 12
// Index of sensors connected to data pin, default: 0
#define SENSOR_INDEX 0
OneWire oneWire(DATA_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress sensorDeviceAddress;

// ############################################################################
// # Define space for dec2strf function
char str[16] = "...";

// ############################################################################
// # Init timer library
Timer t;

void setup()
{

  Serial.println("Setup");
  
  Serial.begin(9600);
  
  if (!driver.init())
    Serial.println("init failed");

  pinMode(led_pin, OUTPUT);
  
  // Temperature Sensor
  sensors.begin();
  sensors.getAddress(sensorDeviceAddress, 0);
  sensors.setResolution(sensorDeviceAddress, SENSOR_RESOLUTION);
  
  // settle
  delay(50);

  // Timers
  int tempEvent = t.every(10000, checkTemp, (void*)2);


}

void loop()
{
  // Update timers
  t.update();
}


void checkTemp(void* context)
{
  String message = "";
 int xx = analogRead(ldr);
 String LDR = String(xx);


  sensors.requestTemperatures();
  float temperatureInCelsius = sensors.getTempCByIndex(SENSOR_INDEX);
  message = "T," + String(temperatureInCelsius,4) + "," + LDR;
  Transmit(message);
  message = "";
}

String Transmit(String data)
{
 
  Serial.println(data);
  
  char copy[50];
  data.toCharArray(copy, 50);
  char *msg = copy;

  digitalWrite(led_pin, HIGH); // Flash a light to show transmitting
  driver.send((uint8_t *)msg, strlen(msg));
  driver.waitPacketSent();
  digitalWrite(led_pin, LOW);
  memset(copy, 0, sizeof(copy));
}
